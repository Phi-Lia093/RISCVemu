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
#include <emu.h>
#include <exec.h>
#include <fmap.h>
#include <logger.h>
#include <mem.h>

#include <config.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
#include <extension/zicsr_extension.h>
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
    uint32_t program_base;
    const char *program_name;

    static struct option long_options[] = {
#ifdef CONFIG_ENABLE_DEBUGGER
        { "debug", no_argument, 0, 'd' },
        { "breakpoint", required_argument, 0, 'b' },
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
#else
    g_state.single_step = 0;
    g_state.breakpoint_enabled = 0;
#endif

    info("starting execution at PC=0x%x", g_state.pc);

#ifdef CONFIG_ENABLE_DEBUGGER
    while (!g_state.terminated)
    {
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

        if (g_state.single_step)
        {
            tick_debugger();
        }
        if (g_state.terminated)
        {
            break;
        }

        uint32_t ins = mem_read32_unsigned(g_state.pc);
        exec(ins);
        g_state.pc += 4;
    }
#else
    while (!g_state.terminated)
    {
        if (g_state.pc >= MEM_SIZE)
        {
            fatal("PC out of bounds: 0x%x", g_state.pc);
            break;
        }

        uint32_t ins = mem_read32_unsigned(g_state.pc);
        exec(ins);
        g_state.pc += 4;
    }
#endif

    info("Execution terminated. Return value: 0x%x (final x10 value) 0x%x "
         "(final x17 value)",
         g_state.gpr[10], g_state.gpr[17]);
    terminate_logger();
    return 0;
}
