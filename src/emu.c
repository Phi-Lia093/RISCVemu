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

#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <debugger.h>
#include <device/clint.h>
#include <device/fdt.h>
#include <emu.h>
#include <exec.h>
#include <fmap.h>
#include <logger.h>
#include <mem.h>
#include <mmu.h>

#include <config.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
#include <extension/sdtrig_extension.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION
#include <extension/zicntr_extension.h>
#endif

struct machine_state g_state = { 0 }; // this will not fuck up addrs

static void
print_usage(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s <program_file> <program_base> [options]\n"
            "Options:\n"
#ifdef CONFIG_ENABLE_DEBUGGER
            "  -d, --debug         Enable single-step debug mode\n"
            "  -b, --breakpoint    Set breakpoint address (hex format, e.g., "
            "0x1000)\n"
#endif
#ifdef CONFIG_ENABLE_FDT
            "  -F, --fdt[=addr]    Build + hand a device tree to firmware "
            "in a1\n"
            "                      (a0=hartid 0); addr defaults to "
            "0x87f00000\n"
#endif
            "  -h, --help          Show this help message\n"
            "\n"
            "Example:\n"
            "  %s test.bin 0x1000 -d -b 0x1200\n",
            prog_name, prog_name);
}

static int
parse_hex(const char *str, uint32_t *result)
{
    char *endptr;
    unsigned long val = strtoul(str, &endptr, 0);

    if (*endptr != '\0' || endptr == str) return -1;

    *result = (uint32_t)val;
    return 0;
}

int
main(int argc, char **argv)
{
    int opt;
#ifdef CONFIG_ENABLE_DEBUGGER
    int debug_mode = 0;
    uint32_t breakpoint = 0;
    int breakpoint_set = 0;
#endif
#ifdef CONFIG_ENABLE_FDT
    int fdt_enabled = 0;
    uint32_t fdt_addr = FDT_DEFAULT_ADDR;
#endif
    uint32_t program_base;
    const char *program_name;

    static struct option long_options[] = {
#ifdef CONFIG_ENABLE_DEBUGGER
        { "debug", no_argument, 0, 'd' },
        { "breakpoint", required_argument, 0, 'b' },
#endif
#ifdef CONFIG_ENABLE_FDT
        { "fdt", optional_argument, 0, 'F' },
#endif
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };

#ifdef CONFIG_ENABLE_DEBUGGER
    while ((opt = getopt_long(argc, argv, "db:h", long_options, NULL)) != -1)
#else
    while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1)
#endif
    {
        switch (opt)
        {
#ifdef CONFIG_ENABLE_DEBUGGER
        case 'd':
            debug_mode = 1;
            break;

        case 'b':
            if (parse_hex(optarg, &breakpoint) != 0)
            {
                fprintf(stderr, "Error: Invalid breakpoint address: %s\n",
                        optarg);
                return 1;
            }
            breakpoint_set = 1;
            break;
#endif

#ifdef CONFIG_ENABLE_FDT
        case 'F':
            fdt_enabled = 1;
            /* --fdt=0x... decodes the explicit placement; --fdt alone keeps
             * the default.  getopt delivers optional_argument via "=" only. */
            if (optarg)
            {
                if (parse_hex(optarg, &fdt_addr) != 0)
                {
                    fprintf(stderr,
                            "Error: Invalid FDT address: %s\n", optarg);
                    return 1;
                }
            }
            break;
#endif

        case 'h':
            print_usage(argv[0]);
            return 0;

        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (optind + 2 > argc)
    {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        return 1;
    }

    program_name = argv[optind];
    if (parse_hex(argv[optind + 1], &program_base) != 0)
    {
        fprintf(stderr, "Error: Invalid program base address: %s\n",
                argv[optind + 1]);
        return 1;
    }

#ifdef CONFIG_ENABLE_DEBUGGER
    init_logger(debug_mode ? DEBUG : INFO, NULL);
#else
    init_logger(INFO, NULL);
#endif
    info("RISC-V Emulator starting...");
    info("Program: %s", program_name);
    info("Base address: 0x%x", program_base);
#ifdef CONFIG_ENABLE_DEBUGGER
    if (breakpoint_set) info("Breakpoint: 0x%x", breakpoint);
#endif

    init_mem();

    size_t len;
    uint8_t *addr
        = (uint8_t *)map_file(program_name, &len, PROT_READ | PROT_WRITE);
    if (!addr)
    {
        fatal("failed to load program: %s", program_name);
    }

    if (program_base + len > MEM_SIZE)
    {
        fatal("program too large for memory (base=0x%x, size=%zu, max=0x%x)",
              program_base, len, MEM_SIZE);
        munmap(addr, len);
        terminate_logger();
        return 1;
    }

    memcpy(g_state.main_memory + program_base, addr, len);
    info("loaded %zu bytes to 0x%x", len, program_base);
    munmap(addr, len);

#ifdef CONFIG_ENABLE_FDT
    // OpenSBI and OS bootloaders expect a flattened device tree handed in a1
    // (and the boot HART id in a0).  This is opt-in via `--fdt[=addr]` because
    // the general test harness also loads at 0x80000000 and must keep a0/a1
    // untouched.  When enabled we build the DTB into the emulated RAM, then
    // prime the boot registers before the firmware runs.
    if (fdt_enabled)
    {
        size_t fdt_size = fdt_build_riscvemu(fdt_addr);
        if (fdt_size == 0)
        {
            fatal("failed to build device tree at 0x%x", fdt_addr);
        }
        if (fdt_addr + fdt_size > MEM_SIZE || fdt_addr < program_base + len)
        {
            fatal("device tree at 0x%x overlaps loaded program", fdt_addr);
        }
        g_state.gpr[10] = 0;        /* a0 = boot HART id (hart 0) */
        g_state.gpr[11] = fdt_addr; /* a1 = pointer to the FDT */
        info("built %zu-byte device tree at 0x%x (a0/a1 supplied)",
             fdt_size, fdt_addr);
    }
#endif

#ifdef CONFIG_ENABLE_DEBUGGER
    init_debugger();
#endif

    // we cannot fill 0 here because we will fuck up the mmu flags addr and main
    // mem addr
    g_state.pc = program_base;
    g_state.terminated = 0;
    g_state.privilege = PRV_MACHINE;

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
    init_csr_table();
#endif

#ifdef CONFIG_ENABLE_DEBUGGER
    g_state.single_step = debug_mode;

    if (breakpoint_set)
    {
        g_state.breakpoint = breakpoint;
        g_state.breakpoint_enabled = 1;
    }
    else
    {
        g_state.breakpoint = 0xDEADBEEF;
        g_state.breakpoint_enabled = 0;
    }
#endif

    info("starting execution at PC=0x%x", g_state.pc);

    // The riscv-tests harness reports the conclusion of a test that traps
    // (e.g. an ECALL swallowed by the trap vector) by spinning forever in the
    // "write_tohost" loop, signalled via the `tohost` symbol. A plain-function
    // emulator has no memory device to observe `tohost`, so we detect this
    // spin loop (a short, repeating PC cycle that makes no forward progress)
    // and terminate. RVTEST_PASS/RVTEST_FAIL have already latched the pass/fail
    // code into the a7/a0 (x17/x10) GPRs, so the final register report is still
    // correct for the existing test harness.
    uint32_t loop_window[8];
    for (int i = 0; i < 8; i++) loop_window[i] = 0;
    uint32_t loop_window_pos = 0;
    int spin_count = 0;
    int spin_armed = 0;
    uint32_t last_fetch_pc = 0;

    // The write_tohost spin detector exists to conclude the riscv-tests
    // harness, which spins forever after swallowing an exception.  OpenSBI and
    // other OS firmware legitimately trap (csr_read_allowed CSR probes) and
    // busy-spin (cpu_relax / coldboot waits), so the detector must be off in
    // FDT/OS-boot mode; otherwise it misreads a healthy boot as a hung test.
    int spin_detect_active = 1;
#ifdef CONFIG_ENABLE_FDT
    if (fdt_enabled)
        spin_detect_active = 0;
#endif

#ifdef CONFIG_ENABLE_DEBUGGER
    while (!g_state.terminated)
    {
        check_and_handle_interrupts();
        if (g_state.pc >= MEM_SIZE)
        {
            fatal("PC out of bounds: 0x%x", g_state.pc);
            break;
        }

        if (g_state.breakpoint_enabled && g_state.pc == g_state.breakpoint)
        {
            info("breakpoint hit at PC=0x%x", g_state.pc);
            g_state.single_step = 1;
            g_state.breakpoint_enabled = 0;
        }

        if (g_state.break_requested)
        {
            g_state.break_requested = 0;
            printf("Received SIGINT, entering debugger.\n");
        }

        if (g_state.single_step)
        {
            tick_debugger();
        }
        if (g_state.terminated)
        {
            break;
        }

        uint32_t fetch_pc = g_state.pc;
        g_prev_ins_pc = last_fetch_pc;
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        uint32_t ins;
        int fetch_exec = 1;
        // Fetch translation (SV32 page tables). On a fetch page fault, mmu
        // raises the exception (which sets pc to the trap vector - 4, so the
        // unconditional pc += 4 below lands exactly on the trap vector).
        if (!mmu_fetch_ok(fetch_pc, &ins))
        {
            fetch_exec = 0;
        }
        else if (sdtrig_check_fetch_trigger(fetch_pc))
        {
            // Fetch breakpoint fired: raise_exception set pc to the trap
            // vector - 4; don't execute, just let pc += 4 complete the trap.
            fetch_exec = 0;
        }
        if (fetch_exec)
        {
            exec(ins);
        }
#else
        uint32_t ins = mem_read32_unsigned(fetch_pc);
        exec(ins);
#endif
        last_fetch_pc = fetch_pc;
        g_state.pc += 4;
        clint_tick();
#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION
        cycle++;
        if (unlikely(instret_suppress_next))
        {
            instret_suppress_next = 0;
        }
        else
        {
            instret++;
        }
#endif
        // Detect the harness "write_tohost" spin loop (armed only once the
        // program traps; the loop is reached after a swallowed exception).
        if (spin_detect_active && g_state.just_trapped)
        {
            g_state.just_trapped = 0;
            spin_armed = 1;
            for (int i = 0; i < 8; i++) loop_window[i] = 0;
            loop_window_pos = 0;
            spin_count = 0;
        }
        if (spin_armed && spin_detect_active)
        {
            int seen = 0;
            for (int i = 0; i < 8; i++)
                if (loop_window[i] == fetch_pc) seen = 1;
            if (seen)
                spin_count++;
            else
                spin_count = 0;
            loop_window[loop_window_pos] = fetch_pc;
            loop_window_pos = (loop_window_pos + 1) % 8;
            if (spin_count >= 64)
            {
                // Entered the harness "write_tohost" loop: the result is
                // latched in the TESTNUM/GPR (gp). Mirror the RVTEST_PASS /
                // RVTEST_FAIL register convention so the test harness sees a
                // proper exit code (a7 = 93, a0 = 0 on pass).
                if (g_state.gpr[3] == 1)
                {
                    g_state.gpr[10] = 0;
                }
                g_state.gpr[17] = 93;
                g_state.terminated = 1;
            }
        }
    }
#else
    while (!g_state.terminated)
    {
        // Dispatch pending interrupts (e.g. CLINT MIP.MTIP) before the next
        // fetch, exactly like the debugger-enabled loop above.  Without this,
        // timer and other interrupts would never be taken when the built-in
        // debugger is compiled out.
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        check_and_handle_interrupts();
#endif
        if (g_state.pc >= MEM_SIZE)
        {
            fatal("PC out of bounds: 0x%x", g_state.pc);
            break;
        }

        uint32_t fetch_pc = g_state.pc;
        g_prev_ins_pc = last_fetch_pc;
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
        uint32_t ins;
        int fetch_exec = 1;
        // Fetch translation (SV32 page tables). On a fetch page fault, mmu
        // raises the exception (which sets pc to the trap vector - 4, so the
        // unconditional pc += 4 below lands exactly on the trap vector).
        if (!mmu_fetch_ok(fetch_pc, &ins))
        {
            fetch_exec = 0;
        }
        else if (sdtrig_check_fetch_trigger(fetch_pc))
        {
            // Fetch breakpoint fired: raise_exception set pc to the trap
            // vector - 4; don't execute, just let pc += 4 complete the trap.
            fetch_exec = 0;
        }
        if (fetch_exec)
        {
            exec(ins);
        }
#else
        uint32_t ins = mem_read32_unsigned(fetch_pc);
        exec(ins);
#endif
        last_fetch_pc = fetch_pc;
        g_state.pc += 4;
        clint_tick();
#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION
        cycle++;
        if (unlikely(instret_suppress_next))
        {
            instret_suppress_next = 0;
        }
        else
        {
            instret++;
        }
#endif
        // Detect the harness "write_tohost" spin loop (armed only once the
        // program traps; the loop is reached after a swallowed exception).
        if (spin_detect_active && g_state.just_trapped)
        {
            g_state.just_trapped = 0;
            spin_armed = 1;
            for (int i = 0; i < 8; i++) loop_window[i] = 0;
            loop_window_pos = 0;
            spin_count = 0;
        }
        if (spin_armed && spin_detect_active)
        {
            int seen = 0;
            for (int i = 0; i < 8; i++)
                if (loop_window[i] == fetch_pc) seen = 1;
            if (seen)
                spin_count++;
            else
                spin_count = 0;
            loop_window[loop_window_pos] = fetch_pc;
            loop_window_pos = (loop_window_pos + 1) % 8;
            if (spin_count >= 64)
            {
                // Entered the harness "write_tohost" loop: the result is
                // latched in the TESTNUM/GPR (gp). Mirror the RVTEST_PASS /
                // RVTEST_FAIL register convention so the test harness sees a
                // proper exit code (a7 = 93, a0 = 0 on pass).
                if (g_state.gpr[3] == 1)
                {
                    g_state.gpr[10] = 0;
                }
                g_state.gpr[17] = 93;
                g_state.terminated = 1;
            }
        }
    }
#endif

    info("Execution terminated. Return value: 0x%x (final x10 value) 0x%x "
         "(final x17 value)",
         g_state.gpr[10], g_state.gpr[17]);
    terminate_logger();
    return 0;
}
