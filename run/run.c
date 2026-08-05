/*
 * SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of RISCVemu.
 * RISCVemu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RISCVemu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Bare-metal CLINT timer self-test.
 *
 * riscv-tests ships no coverage for the Core-Local INTerruptor, so this
 * program directly exercises the emulator's CLINT device and the MIP.MTIP
 * interrupt path.  It validates:
 *   1. mtime (MMIO at 0x0200BFF8) is a free-running counter that advances.
 *   2. The architectural `time` CSR (rdtime) reports the *same* counter as
 *      the MMIO mtime, so an mtimecmp deadline computed from rdtime agrees
 *      with the comparator that drives MIP.MTIP.
 *   3. Writing mtimecmp <= mtime makes MIP.MTIP pending and, with
 *      MSTATUS.MIE + MIE.MTIP set, dispatch raises an M-timer trap whose
 *      mcause is 0x80000007.
 *   4. Re-arming mtimecmp = mtime + delta in the handler produces a periodic
 *      stream of timer interrupts; reading back MIP.MTIP confirms it clears
 *      once mtimecmp is pushed beyond mtime.
 *   5. Writing mtimecmp = 0 disables the compare (no further interrupts).
 *
 * Progress and the PASS/FAIL verdict are printed to the 16550 UART at
 * 0x10000000 (stdout), and _main returns 0 to exit via ecall(a7=93).
 */

#include <stdint.h>

// ---------------------------------------------------------------------------
// Constants (mirror include/device/clint.h and the RISC-V privilege spec)
// ---------------------------------------------------------------------------
#define CLINT_MTIMECMP 0x02004000UL
#define CLINT_MTIME    0x0200BFF8UL

#define UART_THR 0x10000000UL

#define MSTATUS_MIE 0x08U
#define MIP_MTIP    0x80U   /* bit 7 */
#define IRQ_M_TIMER 7

#define TIMER_DELTA 8U       /* mtimecmp = mtime + TIMER_DELTA */
#define TARGET_COUNT 8       /* arm the timer TARGET_COUNT times */

// ---------------------------------------------------------------------------
// Tiny UART console (same output path as the 16550 device).
// ---------------------------------------------------------------------------
static void
debug_print(const char *str)
{
    while (*str)
    {
        *(volatile uint32_t *)UART_THR = (uint32_t)(uint8_t)*str++;
    }
}

static void
print_hex32(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    char buf[3];
    debug_print("0x");
    for (int i = 7; i >= 0; i--)
    {
        buf[0] = digits[(v >> (i * 4)) & 0xF];
        buf[1] = '\0';
        debug_print(buf);
    }
}

void
_start(void) __attribute__((section(".text._start")));

int
_main(void);

// ---------------------------------------------------------------------------
// MMIO helpers.
// ---------------------------------------------------------------------------
static inline uint32_t
mmio_read32(unsigned long addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void
mmio_write32(unsigned long addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint64_t
mmio_read64(unsigned long addr)
{
    uint32_t lo = mmio_read32(addr);
    uint32_t hi = mmio_read32(addr + 4);
    return ((uint64_t)hi << 32) | lo;
}

// Read CLINT mtime (64-bit little-endian MMIO).
static inline uint64_t
clint_read_mtime(void)
{
    return mmio_read64(CLINT_MTIME);
}

// Write CLINT mtimecmp (64-bit).  High word first so the compare is never
// momentarily satisfied while the low word is still being assembled.
static inline void
clint_write_mtimecmp(uint64_t val)
{
    mmio_write32(CLINT_MTIMECMP + 4, (uint32_t)(val >> 32));
    mmio_write32(CLINT_MTIMECMP, (uint32_t)val);
}

// ---------------------------------------------------------------------------
// CSR helpers (token-pasting so the csr name is a literal in the asm).
// ---------------------------------------------------------------------------
#define read_csr(reg)                                                        \
    ({                                                                       \
        unsigned long __tmp;                                                 \
        __asm__ volatile("csrr %0, " #reg : "=r"(__tmp));                    \
        __tmp;                                                               \
    })

#define write_csr(reg, val)                                                  \
    ({                                                                       \
        __asm__ volatile("csrw " #reg ", %0" ::"rK"(val));                   \
    })

#define set_csr(reg, bit)                                                    \
    ({                                                                       \
        unsigned long __tmp;                                                 \
        __asm__ volatile("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
        __tmp;                                                               \
    })

// ---------------------------------------------------------------------------
// Interrupt state shared with the timer_isr().
// ---------------------------------------------------------------------------
static volatile uint32_t timer_count;
static volatile uint32_t bad_cause; // set if an unexpected trap occurs

// Invoked from the assembly trap stub after all GPRs have been saved.  Pure C,
// so it is free to follow the normal calling convention.
__attribute__((used)) static void
timer_isr(uint32_t mcause)
{
    if ((mcause & 0x80000000U) && (mcause & 0x7FFFFFFFU) == IRQ_M_TIMER)
    {
        // MIP.MTIP must be pending on entry to the timer handler.
        if (!(read_csr(mip) & MIP_MTIP))
        {
            bad_cause = 0xDEAD0001U;
        }
        timer_count++;

        // Re-arm one more interval until the main loop has observed the
        // requested number of interrupts; then disable the compare.
        if (timer_count < TARGET_COUNT)
        {
            clint_write_mtimecmp(clint_read_mtime() + TIMER_DELTA);
        }
        else
        {
            clint_write_mtimecmp(0);
        }
    }
    else
    {
        bad_cause = mcause; // unexpected trap
    }
}

// Assembly trap entry: save every GPR but x0, run timer_isr(), restore and
// mret.  This guarantees the interrupted main-loop code is never clobbered.
__attribute__((naked)) static void
timer_trap_stub(void)
{
    __asm__ volatile(
        "addi sp, sp, -128\n\t"
        "sw x1,  0(sp)\n\t"
        "sw x2,  4(sp)\n\t"
        "sw x3,  8(sp)\n\t"
        "sw x4, 12(sp)\n\t"
        "sw x5, 16(sp)\n\t"
        "sw x6, 20(sp)\n\t"
        "sw x7, 24(sp)\n\t"
        "sw x8, 28(sp)\n\t"
        "sw x9, 32(sp)\n\t"
        "sw x10, 36(sp)\n\t"
        "sw x11, 40(sp)\n\t"
        "sw x12, 44(sp)\n\t"
        "sw x13, 48(sp)\n\t"
        "sw x14, 52(sp)\n\t"
        "sw x15, 56(sp)\n\t"
        "sw x16, 60(sp)\n\t"
        "sw x17, 64(sp)\n\t"
        "sw x18, 68(sp)\n\t"
        "sw x19, 72(sp)\n\t"
        "sw x20, 76(sp)\n\t"
        "sw x21, 80(sp)\n\t"
        "sw x22, 84(sp)\n\t"
        "sw x23, 88(sp)\n\t"
        "sw x24, 92(sp)\n\t"
        "sw x25, 96(sp)\n\t"
        "sw x26, 100(sp)\n\t"
        "sw x27, 104(sp)\n\t"
        "sw x28, 108(sp)\n\t"
        "sw x29, 112(sp)\n\t"
        "sw x30, 116(sp)\n\t"
        "sw x31, 120(sp)\n"
        "csrr a0, mcause\n\t"
        "call timer_isr\n\t"
        "lw x31, 120(sp)\n\t"
        "lw x30, 116(sp)\n\t"
        "lw x29, 112(sp)\n\t"
        "lw x28, 108(sp)\n\t"
        "lw x27, 104(sp)\n\t"
        "lw x26, 100(sp)\n\t"
        "lw x25, 96(sp)\n\t"
        "lw x24, 92(sp)\n\t"
        "lw x23, 88(sp)\n\t"
        "lw x22, 84(sp)\n\t"
        "lw x21, 80(sp)\n\t"
        "lw x20, 76(sp)\n\t"
        "lw x19, 72(sp)\n\t"
        "lw x18, 68(sp)\n\t"
        "lw x17, 64(sp)\n\t"
        "lw x16, 60(sp)\n\t"
        "lw x15, 56(sp)\n\t"
        "lw x14, 52(sp)\n\t"
        "lw x13, 48(sp)\n\t"
        "lw x12, 44(sp)\n\t"
        "lw x11, 40(sp)\n\t"
        "lw x10, 36(sp)\n\t"
        "lw x9, 32(sp)\n\t"
        "lw x8, 28(sp)\n\t"
        "lw x7, 24(sp)\n\t"
        "lw x6, 20(sp)\n\t"
        "lw x5, 16(sp)\n\t"
        "lw x4, 12(sp)\n\t"
        "lw x3,  8(sp)\n\t"
        "lw x2,  4(sp)\n\t"
        "lw x1,  0(sp)\n\t"
        "addi sp, sp, 128\n\t"
        "mret\n");
}



// ---------------------------------------------------------------------------
// Test helpers.
// ---------------------------------------------------------------------------
static int ran;

static void
report(int ok, const char *what)
{
    debug_print(ok ? "  [PASS] " : "  [FAIL] ");
    debug_print(what);
    debug_print("\n");
    if (!ok) ran = 0;
}

static int
wait_for_interrupts(void)
{
    // Spin; interrupts are dispatched deterministically by the emulator.
    uint64_t guard = 0;
    while (timer_count < TARGET_COUNT && bad_cause == 0 && guard < 10000000)
    {
        guard++;
    }
    return (int)timer_count;
}

void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n");
    _main();
    __asm__ volatile("li a7, 93; ecall\n");
    while (1) {}
}

int
_main(void)
{
    uint64_t spin;

    ran = 1;
    timer_count = 0;
    bad_cause = 0;

    debug_print("CLINT timer self-test\n");

    // 1) mtime is a free-running counter: a busy loop must advance it.
    uint64_t t0 = clint_read_mtime();
    spin = 0;
    while (spin < 500) spin++;
    uint64_t t1 = clint_read_mtime();
    report(t1 > t0, "mtime advances across a busy loop");

    debug_print("  mtime0=");
    print_hex32((uint32_t)t0);
    debug_print(" mtime1=");
    print_hex32((uint32_t)t1);
    debug_print("\n");

    // 2) The architectural `time` CSR must report the same free-running
    //    counter that drives the CLINT comparator.  Read both back-to-back:
    //    mtime advances once per executed guest instruction, so between the
    //    two reads it can only drift by the small, fixed instruction sequence
    //    in between (a dozen or two).  Allow a generous fixed window: the
    //    mmio value must land just at-or-after the CSR value.  (If `time`
    //    were still sampled from the host wall clock, as it was before the
    //    fix, tmmio in the low thousands would never track tcsr, which sat in
    //    the billions of nanoseconds.)
    uint64_t tcsr = (uint64_t)read_csr(time);
    uint64_t tmmio = clint_read_mtime();
    int aligned = tmmio >= tcsr && (tmmio - tcsr) <= 256U;
    report(aligned, "time CSR (rdtime) tracks CLINT mtime");
    debug_print("  tcsr=");
    print_hex32((uint32_t)tcsr);
    debug_print(" tmmio=");
    print_hex32((uint32_t)tmmio);
    debug_print("\n");

    // 3) Arm the timer and enable interrupts.
    clint_write_mtimecmp(clint_read_mtime() + TIMER_DELTA);
    write_csr(mtvec, (uint32_t)timer_trap_stub);
    set_csr(mie, MIP_MTIP);
    set_csr(mstatus, MSTATUS_MIE);

    // 4) Wait for TARGET_COUNT timer interrupts.
    int got = wait_for_interrupts();
    report(bad_cause == 0, "no unexpected trap taken");
    report(got == TARGET_COUNT,
           "received expected number of M-timer interrupts");
    debug_print("  timer_count=");
    print_hex32((uint32_t)timer_count);
    debug_print(" bad_cause=");
    print_hex32(bad_cause);
    debug_print("\n");

    // 5) With mtimecmp == 0 the compare is disabled: MTIP must clear.
    clint_write_mtimecmp(0);
    report(!(read_csr(mip) & MIP_MTIP),
           "writing mtimecmp=0 clears MIP.MTIP");

    // Disable the interrupt source and M-level enable before finishing.
    write_csr(mie, read_csr(mie) & ~MIP_MTIP);
    write_csr(mstatus, read_csr(mstatus) & ~MSTATUS_MIE);

    debug_print(ran ? "CLINT SELF-TEST PASS\n" : "CLINT SELF-TEST FAIL\n");
    return ran ? 0 : 1;
}
