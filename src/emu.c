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
            "  -L, --load FILE@HEX   Load an extra binary into RAM at the\n"
            "                        given address (repeatable; e.g. the\n"
            "                        OpenSBI next-stage kernel).\n"
            "  -C, --console         UART console mode: the guest console owns\n"
            "                        the host terminal (stdin/stdout), logger\n"
            "                        output is dumped to --log (default\n"
            "                        riscvemu.log), and the built-in debugger\n"
            "                        is disabled.  The host terminal switches\n"
            "                        to raw mode: keystrokes are sent to the\n"
            "                        guest immediately and are not echoed (the\n"
            "                        guest controls the screen).  Press Ctrl-]\n"
            "                        then x to quit console mode.\n"
            "  -l, --log FILE        Write logger output to FILE (truncated).\n"
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
            "  %s test.bin 0x1000 -d -b 0x1200\n"
            "  %s fw_jump.bin 0x80000000 --fdt --load kernel.bin@0x80400000\n",
            prog_name, prog_name, prog_name);
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

/* ------------------------------------------------------------------------- */
/* Extra (second+ stage) binary load slots collected from `--load FILE@HEX`. */
/* ------------------------------------------------------------------------- */
struct extra_load
{
    char *file;    /* owned copy of the file name */
    uint32_t base; /* validated RAM address to place it at */
};

static int
push_extra_load(struct extra_load **slots, unsigned *count, unsigned *cap,
                const char *arg)
{
    /* Split at the address separator: FILE@HEX. */
    const char *at = strrchr(arg, '@');
    if (!at || at == arg || *(at + 1) == '\0')
    {
        fprintf(stderr, "Error: --load expects FILE@HEX, got \"%s\"\n", arg);
        return -1;
    }

    uint32_t base;
    if (parse_hex(at + 1, &base) != 0)
    {
        fprintf(stderr, "Error: invalid address in --load \"%s\"\n", arg);
        return -1;
    }

    if (*count == *cap)
    {
        unsigned new_cap = *cap ? *cap * 2 : 4;
        struct extra_load *grown = realloc(*slots, new_cap * sizeof(**slots));
        if (!grown)
        {
            perror("realloc");
            return -1;
        }
        *slots = grown;
        *cap = new_cap;
    }

    (*slots)[*count].file = strndup(arg, (size_t)(at - arg));
    if (!(*slots)[*count].file)
    {
        perror("strndup");
        return -1;
    }
    (*slots)[*count].base = base;
    (*count)++;
    return 0;
}

static void
free_extra_slots(struct extra_load *slots, unsigned count)
{
    for (unsigned i = 0; i < count; i++) free(slots[i].file);
    free(slots);
}

int
main(int argc, char **argv)
{
    int opt;
    int console_mode = 0; /* UART console mode: guest owns the host terminal */
    const char *log_file
        = NULL; /* --log FILE (defaults to riscvemu.log in console mode) */
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

    // Extra (second+ stage) binaries staged via one or more `--load FILE@HEX`.
    struct extra_load *extra_slots = NULL;
    unsigned extra_count = 0;
    unsigned extra_cap = 0;

    static struct option long_options[]
        = { { "load", required_argument, 0, 'L' },
#ifdef CONFIG_ENABLE_DEBUGGER
            { "debug", no_argument, 0, 'd' },
            { "breakpoint", required_argument, 0, 'b' },
#endif
            { "console", no_argument, 0, 'C' },
            { "log", required_argument, 0, 'l' },
#ifdef CONFIG_ENABLE_FDT
            { "fdt", optional_argument, 0, 'F' },
#endif
            { "help", no_argument, 0, 'h' },
            { 0, 0, 0, 0 } };

#ifdef CONFIG_ENABLE_DEBUGGER
    while ((opt = getopt_long(argc, argv, "LCdb:hl:", long_options, NULL))
           != -1)
#else
    while ((opt = getopt_long(argc, argv, "LChl:", long_options, NULL)) != -1)
#endif
    {
        switch (opt)
        {
        case 'L':
            if (push_extra_load(&extra_slots, &extra_count, &extra_cap, optarg)
                != 0)
            {
                free_extra_slots(extra_slots, extra_count);
                return 1;
            }
            break;

        case 'C':
            console_mode = 1;
            break;

        case 'l':
            log_file = optarg;
            break;

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
                    fprintf(stderr, "Error: Invalid FDT address: %s\n", optarg);
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

    /* --console: the guest UART owns the host terminal, so logger output is
     * dumped to a file (riscvemu.log unless --log FILE) and nothing else
     * touches stdout.  Without --console, logs stay on stdout (and go to a
     * file too when --log is given). */
    const char *log_file_path = NULL;
    int log_to_stdout = 1;
    if (console_mode)
    {
        log_file_path = log_file != NULL ? log_file : "riscvemu.log";
        log_to_stdout = 0;
    }
    else if (log_file != NULL)
    {
        log_file_path = log_file;
    }
#ifdef CONFIG_ENABLE_DEBUGGER
    init_logger(console_mode ? INFO : (debug_mode ? DEBUG : INFO),
                log_file_path, log_to_stdout);
#else
    init_logger(INFO, log_file_path, log_to_stdout);
#endif
    info("RISC-V Emulator starting...");
    info("Program: %s", program_name);
    info("Base address: 0x%x", program_base);
#ifdef CONFIG_ENABLE_DEBUGGER
    if (console_mode && (debug_mode || breakpoint_set))
    {
        info("--console: built-in debugger disabled (-d/-b ignored)");
    }
    if (breakpoint_set && !console_mode) info("Breakpoint: 0x%x", breakpoint);
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

    // Extra (e.g. OpenSBI next-stage) binaries are staged later, once the
    // device tree address/size are known so overlaps can be rejected (see
    // the `--load` block after the FDT section).

#ifdef CONFIG_ENABLE_FDT
    // OpenSBI and OS bootloaders expect a flattened device tree handed in a1
    // (and the boot HART id in a0).  This is opt-in via `--fdt[=addr]` because
    // the general test harness also loads at 0x80000000 and must keep a0/a1
    // untouched.  When enabled we build the DTB into the emulated RAM, then
    // prime the boot registers before the firmware runs.
    size_t fdt_size = 0;
    if (fdt_enabled)
    {
        fdt_size = fdt_build_riscvemu(fdt_addr);
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
        info("built %zu-byte device tree at 0x%x (a0/a1 supplied)", fdt_size,
             fdt_addr);
    }
#endif

    // Stage any extra (e.g. OpenSBI next-stage) binaries.  Each is placed at
    // its requested address; nothing here assumes a particular layout, so any
    // secondary boot stage can be parked ahead of the firmware's hand-off
    // point.  Ranges are bounds- and overlap-checked (including against the
    // device tree, which is already resident when fdt_enabled).
    for (unsigned i = 0; i < extra_count; i++)
    {
        size_t elen;
        uint8_t *eaddr = (uint8_t *)map_file(extra_slots[i].file, &elen,
                                             PROT_READ | PROT_WRITE);
        if (!eaddr)
        {
            fatal("failed to load --load binary: %s", extra_slots[i].file);
        }
        uint32_t base = extra_slots[i].base;
        if (base + elen > MEM_SIZE)
        {
            fatal("extra binary \"%s\" too large for memory (base=0x%x, "
                  "size=%zu, max=0x%x)",
                  extra_slots[i].file, base, elen, MEM_SIZE);
            munmap(eaddr, elen);
            terminate_logger();
            return 1;
        }
#ifdef CONFIG_ENABLE_FDT
        if (fdt_enabled && base < fdt_addr + fdt_size && fdt_addr < base + elen)
        {
            fatal("extra binary \"%s\" at 0x%x overlaps the device tree at "
                  "0x%x (size=%zu)",
                  extra_slots[i].file, base, fdt_addr, fdt_size);
        }
#endif
        memcpy(g_state.main_memory + base, eaddr, elen);
        info("loaded extra %zu bytes to 0x%x (from %s)", elen, base,
             extra_slots[i].file);
        munmap(eaddr, elen);
    }
    free_extra_slots(extra_slots, extra_count);
    extra_slots = NULL;

#ifdef CONFIG_ENABLE_DEBUGGER
    /* --console bypasses the built-in debugger: no SIGINT interception and no
     * stdin reads, so the guest UART exclusively owns the host terminal. */
    if (!console_mode) init_debugger();
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
    g_state.single_step = !console_mode && debug_mode;

    if (breakpoint_set && !console_mode)
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

#ifdef CONFIG_ENABLE_UART_DEVICE
    /* Enable async host-stdin RX before the guest runs.  With the built-in
     * debugger compiled in, stdin is only claimed for the guest UART in
     * --console mode: the debugger blocks on stdin (fgets) and a non-blocking
     * fd would make its prompt spin. */
    int uart_input_enabled = 1;
#ifdef CONFIG_ENABLE_DEBUGGER
    if (!console_mode) uart_input_enabled = 0;
#endif
    if (uart_input_enabled) uart_input_setup(console_mode);
#endif

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
    if (fdt_enabled) spin_detect_active = 0;
#endif

#ifdef CONFIG_ENABLE_DEBUGGER
    while (!g_state.terminated)
    {
        check_and_handle_interrupts();
#ifdef CONFIG_ENABLE_UART_DEVICE
        if (uart_poll_input()) /* Ctrl-] x quit escape / terminating signal */
        {
            uart_input_cleanup();
            break;
        }
#endif
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
#ifdef CONFIG_ENABLE_UART_DEVICE
        uart_tick();
#endif
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
#ifdef CONFIG_ENABLE_UART_DEVICE
        if (uart_poll_input()) /* Ctrl-] x quit escape / terminating signal */
        {
            uart_input_cleanup();
            break;
        }
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
#ifdef CONFIG_ENABLE_UART_DEVICE
        uart_tick();
#endif
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

#ifdef CONFIG_ENABLE_UART_DEVICE
    /* Restore the host terminal before returning (also covered by atexit). */
    uart_input_cleanup();
#endif

    info("Execution terminated. Return value: 0x%x (final x10 value) 0x%x "
         "(final x17 value)",
         g_state.gpr[10], g_state.gpr[17]);
    terminate_logger();
    return 0;
}
