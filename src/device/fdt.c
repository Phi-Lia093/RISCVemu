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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <config.h>

#ifdef CONFIG_ENABLE_FDT

#include <device/clint.h>
#include <device/fdt.h>
#include <emu.h>
#ifdef CONFIG_ENABLE_PLIC_DEVICE
#include <device/plic.h>
#endif
#ifdef CONFIG_ENABLE_UART_DEVICE
#include <device/uart.h>
#endif

/*
 * Flattened Device Tree (DTB) support.
 *
 * This module is a small, dependency-free FDT *writer*.  It serialises a
 * big-endian DTB (the format OpenSBI, U-Boot and Linux all consume) into a
 * buffer, exposes a small generic node/property API, and provides a machine
 * description for RISCVemu that matches the devices it actually emulates.
 *
 * Layout inside the output buffer (see FDT spec / dtc):
 *   [0..40)            header        (10 x big-endian 32-bit words)
 *   [40..)             reserve map   (array of {u64 addr, u64 size},
 *                                     terminated by {0,0})
 *   [struct_base..)     structure block (tokens, 4-byte aligned primitives)
 *   [strings_base..)   strings block   (NUL-terminated property names)
 *
 * We reserve a fixed 4 KiB structure window so structure tokens and property
 * names each form one contiguous run; both are big-endian.
 */

#define FDT_MAGIC 0xd00dfeedu
#define FDT_VERSION 17u
#define FDT_LAST_COMP_VERSION 16u

#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE 0x2u
#define FDT_PROP 0x3u
#define FDT_NOP 0x4u
#define FDT_END 0x9u

#define FDT_HEADER_SIZE 40u /* 10 x 32-bit words */

#define FDT_STRUCT_MAX 4096u /* conservative bound for this small machine */
#define FDT_MAX_CELLS 4u
#define FDT_MAX_PROP_BYTES (FDT_MAX_CELLS * sizeof(uint32_t))

#define FDT_MAX_BUF (FDT_STRUCT_MAX * 4u) /* 16 KiB working region */

/* ------------------------------------------------------------------ */
/* Big-endian byte helpers (host is little-endian).                   */
/* ------------------------------------------------------------------ */

static void
be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static void
be64(uint8_t *p, uint64_t v)
{
    be32(p, (uint32_t)(v >> 32));
    be32(p + 4, (uint32_t)(v & 0xFFFFFFFFu));
}

/* ------------------------------------------------------------------ */
/* Builder bookkeeping.                                               */
/* ------------------------------------------------------------------ */

/* Absolute offset where the structure block lives. */
static uint32_t
struct_base(struct fdt_builder *b)
{
    /* header + a single {0,0} reserve-map terminator (16 bytes) */
    (void)b;
    return FDT_HEADER_SIZE + 16u;
}

/* Absolute offset where the strings block lives (after the struct window). */
static uint32_t
strings_base(struct fdt_builder *b)
{
    return struct_base(b) + FDT_STRUCT_MAX;
}

static void
struct_word(struct fdt_builder *b, uint32_t w)
{
    be32(b->buf + b->struct_off, w);
    b->struct_off += 4u;
}

static void
struct_bytes(struct fdt_builder *b, const void *data, uint32_t len)
{
    if (len) memcpy(b->buf + b->struct_off, data, len);
    b->struct_off += len;
}

/* Pad the structure cursor to a 4-byte boundary with NUL filler. */
static void
struct_align(struct fdt_builder *b)
{
    while (b->struct_off & 0x3u)
    {
        b->buf[b->struct_off] = 0;
        b->struct_off += 1u;
    }
}

/* Append a NUL-terminated string to the structure block, padded to 4 bytes. */
static void
struct_string(struct fdt_builder *b, const char *s)
{
    uint32_t n = (uint32_t)strlen(s);
    memcpy(b->buf + b->struct_off, s, n);
    b->struct_off += n;
    b->buf[b->struct_off] = '\0';
    b->struct_off += 1u;
    struct_align(b);
}

/* Intern a property name in the strings block.  Returns the offset RELATIVE to
 * the start of the strings block (the value stored in a property's nameoff
 * field); libfdt recomputes off_dt_strings + nameoff from this. */
static uint32_t
strings_intern(struct fdt_builder *b, const char *name)
{
    uint32_t rel = b->strings_off - strings_base(b);
    uint32_t n = (uint32_t)strlen(name) + 1u;
    memcpy(b->buf + b->strings_off, name, n);
    b->strings_off += n;
    return rel;
}

/* ------------------------------------------------------------------ */
/* Public builder API.                                                */
/* ------------------------------------------------------------------ */

fdt_handle_t
fdt_builder_init(struct fdt_builder *b)
{
    if (!b || !b->buf) return 0;

    b->struct_off = struct_base(b);
    b->strings_off = strings_base(b);
    b->depth = 0;
    b->err = 0;
    b->rsvmap_count = 0;

    /* The structure block begins with the root node: an explicit FDT_BEGIN_NODE
     * token carrying an *empty* name (dtc emits this for "/").  libfdt walks
     * from off_dt_struct and insists the first tag there is a node, so this
     * empty-name opens the tree without a matching fdt_node_push(). */
    struct_word(b, FDT_BEGIN_NODE);
    struct_string(b, ""); /* empty root name, padded to alignment */
    return (fdt_handle_t)(uintptr_t)b;
}

fdt_handle_t
fdt_node_push(fdt_handle_t h, const char *name)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;

    if (!name || !*name || !b || b->err) return 0;

    struct_word(b, FDT_BEGIN_NODE);
    struct_string(b, name);
    b->depth++;
    return h;
}

fdt_handle_t
fdt_node_pop(fdt_handle_t h)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;

    if (!b || b->err || !b->depth) return 0;
    struct_word(b, FDT_END_NODE);
    b->depth--;
    return h;
}

/*
 * Emit the FDT_PROP header (token, len, nameoff) for a property named `name`
 * whose value will be written by the caller via struct_bytes.  Returns non-zero
 * (a guard) on success so callers can cheaply propagate errors.
 */
static int
prop_begin(struct fdt_builder *b, const char *name, uint32_t len)
{
    if (!name || !b || b->err) return 0;
    struct_word(b, FDT_PROP);
    struct_word(b, len);
    struct_word(b, strings_intern(b, name));
    return 1;
}

fdt_handle_t
fdt_prop_word(fdt_handle_t h, const char *name, uint32_t v0, uint32_t v1,
              uint32_t v2, uint32_t v3)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;
    uint8_t vals[FDT_MAX_PROP_BYTES];

    if (!prop_begin(b, name, sizeof(vals))) return 0;
    be32(&vals[0], v0);
    be32(&vals[4], v1);
    be32(&vals[8], v2);
    be32(&vals[12], v3);
    struct_bytes(b, vals, sizeof(vals));
    return h;
}

/*
 * Emit a property whose value is `n` big-endian 32-bit cells.  Keep the length
 * in the PROP header exact (n * 4 bytes) so libfdt's fdt_cells() - which
 * requires #address-cells / #size-cells to be a single 4-byte cell - accepts
 * them.  This is what makes the 1/2/3-cell helpers correct vs. fdt_prop_word
 * (which always writes four cells / 16 bytes).
 */
static fdt_handle_t
fdt_prop_ncells(fdt_handle_t h, const char *name, const uint32_t *vals,
                unsigned int n)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;
    unsigned int i;

    if (!n || n > FDT_MAX_CELLS) return 0;
    if (!prop_begin(b, name, n * sizeof(uint32_t))) return 0;
    for (i = 0; i < n; i++)
    {
        uint8_t c[4];
        be32(c, vals[i]);
        struct_bytes(b, c, sizeof(c));
    }
    return h;
}

fdt_handle_t
fdt_prop_cells1(fdt_handle_t h, const char *name, uint32_t v0)
{
    const uint32_t vals[1] = { v0 };
    return fdt_prop_ncells(h, name, vals, 1);
}

fdt_handle_t
fdt_prop_cells2(fdt_handle_t h, const char *name, uint32_t v0, uint32_t v1)
{
    const uint32_t vals[2] = { v0, v1 };
    return fdt_prop_ncells(h, name, vals, 2);
}

fdt_handle_t
fdt_prop_cells3(fdt_handle_t h, const char *name, uint32_t v0, uint32_t v1,
                uint32_t v2)
{
    const uint32_t vals[3] = { v0, v1, v2 };
    return fdt_prop_ncells(h, name, vals, 3);
}

fdt_handle_t
fdt_prop_str(fdt_handle_t h, const char *name, const char *str)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;
    uint32_t n = (uint32_t)strlen(str) + 1u; /* include NUL */

    if (!prop_begin(b, name, n)) return 0;
    struct_bytes(b, str, n);
    struct_align(b); /* pad property value to a 4-byte boundary */
    return h;
}

fdt_handle_t
fdt_prop_empty(fdt_handle_t h, const char *name)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;

    if (!prop_begin(b, name, 0)) return 0;
    return h;
}

fdt_handle_t
fdt_prop_bytes(fdt_handle_t h, const char *name, const void *data, size_t len)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;
    uint32_t l = (uint32_t)len;

    if (!prop_begin(b, name, l)) return 0;
    if (l) struct_bytes(b, data, l);
    struct_align(b); /* pad property value to a 4-byte boundary */
    return h;
}

fdt_handle_t
fdt_prop_interrupt_parent(fdt_handle_t h, uint32_t phandle)
{
    return fdt_prop_cells1(h, "interrupt-parent", phandle);
}

fdt_handle_t
fdt_mark_enabled(fdt_handle_t h)
{
    return fdt_prop_str(h, "status", "okay");
}

/* ------------------------------------------------------------------ */
/* Serialisation.                                                     */
/* ------------------------------------------------------------------ */

size_t
fdt_serialize(fdt_handle_t h)
{
    struct fdt_builder *b = (struct fdt_builder *)(uintptr_t)h;
    uint32_t off_struct, off_strings, struct_len, strings_len, totalsize;

    if (!b || b->err || b->depth != 0) return 0;

    /* Close the root node (opened implicitly by fdt_builder_init) with an
     * FDT_END_NODE token, then the whole structure with FDT_END. */
    struct_word(b, FDT_END_NODE);
    struct_align(b);
    struct_word(b, FDT_END);

    off_struct = struct_base(b);
    off_strings = strings_base(b);
    struct_len = b->struct_off - off_struct;
    strings_len = b->strings_off - off_strings;
    totalsize = off_strings + strings_len;

    if (totalsize > FDT_MAX_BUF)
    {
        b->err = 1;
        return 0;
    }

    /* Header words (all big-endian). */
    be32(&b->buf[0], FDT_MAGIC);
    be32(&b->buf[4], totalsize);
    be32(&b->buf[8], off_struct);       /* off_dt_struct */
    be32(&b->buf[12], off_strings);     /* off_dt_strings */
    be32(&b->buf[16], FDT_HEADER_SIZE); /* off_mem_rsvmap */
    be32(&b->buf[20], FDT_VERSION);
    be32(&b->buf[24], FDT_LAST_COMP_VERSION);
    be32(&b->buf[28], FDT_HART_BASE); /* boot_cpuid_phys = hartid 0 */
    be32(&b->buf[32], strings_len);   /* size_dt_strings */
    be32(&b->buf[36], struct_len);    /* size_dt_struct */

    /* Reserve map: exactly one {0,0} terminator entry. */
    {
        uint8_t *r = b->buf + FDT_HEADER_SIZE;
        be64(r, 0);
        be64(r + 8, 0);
    }

    /* Zero the unused (but reserved) gap between the last structure byte and
     * the start of the strings block for deterministic output. */
    {
        uint32_t i;
        for (i = b->struct_off; i < off_strings; i++) b->buf[i] = 0;
    }

    return (size_t)totalsize;
}

/* ------------------------------------------------------------------ */
/* Machine description.                                               */
/* ------------------------------------------------------------------ */

/* phandle handed to /cpus/cpu@0/interrupt-controller. */
#define FDT_CPU_INTC_PHANDLE 1u

/* phandle handed to /soc/plic@... (external interrupt controller). */
#define FDT_PLIC_PHANDLE 2u

/* RISC-V standard local interrupt numbers (dt-binding: sifive,clint). */
#define IRQ_M_SOFT 3u
#define IRQ_M_TIMER 7u
#define IRQ_M_EXT 11u
#define IRQ_S_EXT 9u

/* Free-running CLINT counter frequency reported to firmware (per tick). */
#define FDT_TIMEBASE_FREQ 10000000u

/* RAM region handed to the guest (mirrors QEMU virt for fw_jump). */
#define FDT_RAM_START 0x80000000u
#define FDT_RAM_SIZE 0x80000000u

/*
 * Add the /soc/clint@... node describing the emulated CLINT to the currently
 * open /soc node.  OpenSBI's fdt_timer_mtimer + fdt_ipi_mswi drivers match the
 * "sifive,clint0" compatible and, given our base, derive mtimecmp=+0x4000 and
 * mtime=+0xbff8, exactly matching CLINT_BASE + CLINT_MTIMECMP_HART0 / MTIME.
 * The interrupts-extended cells associate both the machine-software (IPI) and
 * machine-timer lines with the single hart's local interrupt controller.
 */
static fdt_handle_t
fdt_build_clint(fdt_handle_t h)
{
    h = fdt_node_push(h, "clint@2000000");
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "sifive,clint0");
    if (!h) return 0;
    /* reg = <0x0 0x02000000 0x0 0x10000> under 2/2-celled /soc */
    h = fdt_prop_word(h, "reg", 0x0, CLINT_BASE, 0x0, CLINT_SIZE);
    if (!h) return 0;
    /* interrupts-extended = <&cpu_intc 3> <&cpu_intc 7>. */
    h = fdt_prop_word(h, "interrupts-extended", FDT_CPU_INTC_PHANDLE,
                      IRQ_M_SOFT, FDT_CPU_INTC_PHANDLE, IRQ_M_TIMER);
    if (!h) return 0;
    h = fdt_node_pop(h);
    return h;
}

#ifdef CONFIG_ENABLE_PLIC_DEVICE
/*
 * Add the /soc/plic@... node describing the externally-mapped interrupt
 * controller (0x0C000000).  Both the machine external line (IRQ_M_EXT, 11) and
 * the supervisor external line (IRQ_S_EXT, 9) are connected to the sole hart
 * local interrupt controller, matching OpenSBI's and Linux's riscv,plic0
 * expectations so the UART's interrupt line can be wired correctly.
 */
static fdt_handle_t
fdt_build_plic(fdt_handle_t h)
{
    h = fdt_node_push(h, "plic@c000000");
    if (!h) return 0;
    /* compatible = "sifive,plic-1.0.0", "riscv,plic0" (single string-list). */
    {
        static const char plic_comp[] = "sifive,plic-1.0.0\0riscv,plic0\0";
        h = fdt_prop_bytes(h, "compatible", plic_comp, sizeof(plic_comp));
        if (!h) return 0;
    }
    h = fdt_prop_word(h, "reg", 0x0, PLIC_BASE, 0x0, PLIC_SIZE);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#address-cells", 0);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#interrupt-cells", 1);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "phandle", FDT_PLIC_PHANDLE);
    if (!h) return 0;
    h = fdt_prop_empty(h, "interrupt-controller");
    if (!h) return 0;
    /* Highest external interrupt source index exposed (linux/riscv_plic reads
     * this to size its priority/enable arrays). */
    h = fdt_prop_cells1(h, "riscv,ndev", PLIC_NDEV);
    if (!h) return 0;
    /* interrupts-extended = <&cpu_intc 11> <&cpu_intc 9> */
    h = fdt_prop_word(h, "interrupts-extended", FDT_CPU_INTC_PHANDLE, IRQ_M_EXT,
                      FDT_CPU_INTC_PHANDLE, IRQ_S_EXT);
    if (!h) return 0;
    h = fdt_node_pop(h);
    return h;
}
#endif /* CONFIG_ENABLE_PLIC_DEVICE */

#ifdef CONFIG_ENABLE_UART_DEVICE
/*
 * Add the /soc/uart@... node describing the emulated 16550 UART to the
 * currently open /soc node.  OpenSBI's fdt_serial_uart8250 driver matches the
 * "ns16550a" compatible.  The clock-frequency and current-speed properties let
 * the driver compute the baud divisor; LSR.bit5 (THRE) is reported set by the
 * emulator so the outgoing console works.
 */
static fdt_handle_t
fdt_build_uart(fdt_handle_t h)
{
    h = fdt_node_push(h, "uart@10000000");
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "ns16550a");
    if (!h) return 0;
    /* reg = <0x0 0x10000000 0x0 0x8> under 2/2-celled /soc */
    h = fdt_prop_word(h, "reg", 0x0, UART_BASE, 0x0, UART_NREGS);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "clock-frequency", 3686400);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "current-speed", 115200);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "reg-shift", 0);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "reg-io-width", 1);
    if (!h) return 0;
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    /* Bind the UART to the PLIC's external interrupt line 10, giving the
     * 8250 driver a valid IRQ so open("/dev/ttyS0") succeeds and RX data can
     * drive an interrupt. */
    h = fdt_prop_interrupt_parent(h, FDT_PLIC_PHANDLE);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "interrupts", UART_IRQ);
    if (!h) return 0;
#endif
    h = fdt_node_pop(h);
    return h;
}
#endif /* CONFIG_ENABLE_UART_DEVICE */

/*
 * Build the complete machine FDT and write it into g_state.main_memory at
 * `addr`.  The tree (RV32, single hart) mirrors QEMU "virt":
 *
 *   / { #address-cells=<2>; #size-cells=<2>;
 *       compatible="riscvemu"; model="RISCVemu RV32 virt";
 *       chosen {};
 *       memory@80000000 { device_type="memory";
 *                         reg=<0 0x80000000 0 0x80000000>; };
 *       cpus { #address-cells=<1>; #size-cells=<0>;
 *              timebase-frequency=<FDT_TIMEBASE_FREQ>;
 *              cpu@0 { device_type="cpu"; reg=<0>; compatible="riscv";
 *                      riscv,isa="rv32...";
 *                      interrupt-controller { phandle=1; ... }; }; };
 *       soc { compatible="simple-bus"; #address-cells=<2>; #size-cells=<2>;
 *             ranges; clint@... uart@... };
 *   }
 */
size_t
fdt_build_riscvemu(uint32_t addr)
{
    struct fdt_builder b;
    fdt_handle_t h;
    size_t size;

    if (!g_state.main_memory) return 0;

    memset(&b, 0, sizeof(b));
    b.buf = g_state.main_memory + addr;
    b.bufsize = FDT_MAX_BUF;

    h = fdt_builder_init(&b);
    if (!h) return 0;

    /* --- Root "/" properties --- */
    h = fdt_prop_cells1(h, "#address-cells", 2);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#size-cells", 2);
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "riscvemu");
    if (!h) return 0;
    h = fdt_prop_str(h, "model", "RISCVemu RV32 virt");
    if (!h) return 0;

    /* --- /chosen --- */
    h = fdt_node_push(h, "chosen");
    if (!h) return 0;
    /* stdout-path points OpenSBI's fdt_serial_init() at the 16550 console so
     * sbi_printf output reaches the emulated UART instead of being dropped. */
    h = fdt_prop_str(h, "stdout-path", "/soc/uart@10000000");
    if (!h) return 0;
    /* Pass a kernel command line that enables earlycon (sourced from
     * stdout-path) plus the real 8250 console, so Linux prints its banner and
     * any earlier panic message to the emulated UART. */
    h = fdt_prop_str(h, "bootargs",
                     "earlycon console=ttyS0,115200n8 root=/dev/ram0 rw "
                     "rdinit=/init");
    if (!h) return 0;
    h = fdt_node_pop(h);
    if (!h) return 0;

    /* --- /memory@80000000 --- */
    h = fdt_node_push(h, "memory@80000000");
    if (!h) return 0;
    h = fdt_prop_str(h, "device_type", "memory");
    if (!h) return 0;
    h = fdt_prop_word(h, "reg", 0x0, FDT_RAM_START, 0x0, FDT_RAM_SIZE);
    if (!h) return 0;
    h = fdt_node_pop(h);
    if (!h) return 0;

    /* --- /cpus ------------ */
    h = fdt_node_push(h, "cpus");
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#address-cells", 1);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#size-cells", 0);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "timebase-frequency", FDT_TIMEBASE_FREQ);
    if (!h) return 0;

    /* --- /cpus/cpu@0 ------ */
    h = fdt_node_push(h, "cpu@0");
    if (!h) return 0;
    h = fdt_prop_str(h, "device_type", "cpu");
    if (!h) return 0;
    h = fdt_prop_cells1(h, "reg", FDT_HART_BASE);
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "riscv");
    if (!h) return 0;
    /* OpenSBI's fdt_cpu_fixup disables any HART DT node lacking an mmu-type
     * property (it treats a missing/bad mmu-type as "MMU not available" and
     * writes status="disabled").  Without this, the boot hart is disabled,
     * Linux drops cpu@0, the riscv,cpu-intc parent walk fails, and init_IRQ
     * panics "No interrupt controller found".  This machine implements SV32.
     */
    h = fdt_prop_str(h, "mmu-type", "riscv,sv32");
    if (!h) return 0;
    h = fdt_prop_str(h, "riscv,isa", "rv32imafdqc_zicsr_zifencei_zicond");
    if (!h) return 0;
    /* Modern DT interface (Linux 6.18+ requires these for CPU discovery;
     * CONFIG_RISCV_ISA_FALLBACK is typically off, so the old `riscv,isa`
     * string alone is not enough and the boot CPU would be rejected. */
    h = fdt_prop_str(h, "riscv,isa-base", "rv32i");
    if (!h) return 0;
    {
        /* riscv,isa-extensions is a DT string-list: NUL-terminated strings. */
        static const char ext[] = "i\0m\0a\0f\0d\0q\0c\0zicsr\0zifencei\0zicntr\0zicond\0";
        h = fdt_prop_bytes(h, "riscv,isa-extensions", ext, sizeof(ext));
        if (!h) return 0;
    }

    /* --- /cpus/cpu@0/interrupt-controller --- */
    h = fdt_node_push(h, "interrupt-controller");
    if (!h) return 0;
    h = fdt_prop_cells1(h, "phandle", FDT_CPU_INTC_PHANDLE);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#interrupt-cells", 1);
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "riscv,cpu-intc");
    if (!h) return 0;
    h = fdt_prop_empty(h, "interrupt-controller");
    if (!h) return 0;
    h = fdt_node_pop(h); /* interrupt-controller */
    if (!h) return 0;
    h = fdt_node_pop(h); /* cpu@0 */
    if (!h) return 0;
    h = fdt_node_pop(h); /* cpus */
    if (!h) return 0;

    /* --- /soc ------------ */
    h = fdt_node_push(h, "soc");
    if (!h) return 0;
    h = fdt_prop_str(h, "compatible", "simple-bus");
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#address-cells", 2);
    if (!h) return 0;
    h = fdt_prop_cells1(h, "#size-cells", 2);
    if (!h) return 0;
    h = fdt_prop_empty(h, "ranges");
    if (!h) return 0;

    /* Modelled devices.  New devices are added here (or via a small helper)
     * as one fdt_build_* call inside the open /soc node. */
    h = fdt_build_clint(h);
    if (!h) return 0;
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    h = fdt_build_plic(h);
    if (!h) return 0;
#endif
#ifdef CONFIG_ENABLE_UART_DEVICE
    h = fdt_build_uart(h);
    if (!h) return 0;
#endif

    h = fdt_node_pop(h); /* soc */
    if (!h) return 0;

    size = fdt_serialize(h);
    return size;
}

#endif /* CONFIG_ENABLE_FDT */
