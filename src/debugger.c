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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <debugger.h>
#include <disasm.h>
#include <emu.h>
#include <mem.h>

#include <config.h>

#ifdef CONFIG_ENABLE_DEBUGGER

static char last_cmd[256] = { 0 };

static int show_disasm = 1; // 1 = show disassembly, 0 = show raw hex

static void cmd_set_pc(char *args);
static void cmd_set_register(char *args);
static void cmd_single_step(void);
static void cmd_continue(void);
static void cmd_quit(void);
static void cmd_registers(void);
static void cmd_memory(char *args);
static void cmd_write(char *args);
static void cmd_dump_bytes(char *args);
static void cmd_string(char *args);
static void cmd_dump_range(char *args);
static void cmd_breakpoint_set(char *args);
static void cmd_breakpoint_clear(void);
static void cmd_help(void);
static void cmd_disasm(char *args);
static void cmd_toggle_disasm(void);

void
init_debugger(void)
{
    last_cmd[0] = '\0';
    show_disasm = 1;
}

static void
print_registers(void)
{
    const char *reg_names[]
        = { "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
            "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
            "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
            "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

    printf("PC=0x%08x\n", g_state.pc);
    printf("REGISTERS:\n");

    for (int i = 0; i < 32; i++)
    {
        printf("%5s=0x%08x ", reg_names[i], g_state.gpr[i]);
        if ((i + 1) % 4 == 0) printf("\n");
    }
}

static void
cmd_single_step(void)
{
    g_state.single_step = 1;
}

static void
cmd_continue(void)
{
    g_state.single_step = 0;
}

static void
cmd_quit(void)
{
    g_state.terminated = 1;
}

static void
cmd_registers(void)
{
    print_registers();
}

static void
cmd_memory(char *args)
{
    uint32_t addr;
    char *token = strtok(args, " \t\n");

    if (!token)
    {
        printf("usage: m <address> [address2] [address3] ...\n");
        return;
    }

    while (token)
    {
        if (sscanf(token, "%x", &addr) == 1)
        {
            printf("0x%08x: 0x%08x\n", addr, mem_read32_unsigned(addr));
        }
        else
        {
            printf("invalid address: %s\n", token);
        }
        token = strtok(NULL, " \t\n");
    }
}

static void
cmd_write(char *args)
{
    uint32_t addr, val;
    char *token = strtok(args, " \t\n");

    if (!token)
    {
        printf("usage: w <address> <value> [value2] ...\n");
        return;
    }

    if (sscanf(token, "%x", &addr) != 1)
    {
        printf("invalid address: %s\n", token);
        return;
    }

    token = strtok(NULL, " \t\n");
    while (token)
    {
        if (sscanf(token, "%x", &val) == 1)
        {
            mem_write32(addr, val);
            printf("0x%08x <- 0x%08x\n", addr, val);
            addr += 4;
        }
        else
        {
            printf("invalid value: %s\n", token);
        }
        token = strtok(NULL, " \t\n");
    }
}

static void
cmd_dump_bytes(char *args)
{
    uint32_t addr;
    int count = 16;

    if (sscanf(args, "%x %d", &addr, &count) < 1)
    {
        printf("usage: B <address> [count]\n");
        return;
    }

    if (count > 256) count = 256;

    for (int i = 0; i < count; i++)
    {
        if (i % 8 == 0)
        {
            if (i > 0) printf("\n");
            printf("0x%08x: ", addr + i);
        }
        printf("%02x ", mem_read8_unsigned(addr + i));
    }
    printf("\n");
}

static void
cmd_string(char *args)
{
    uint32_t addr;

    if (sscanf(args, "%x", &addr) == 1)
    {
        char c;
        printf("\"");
        while ((c = mem_read8_unsigned(addr++)) && c >= ' ')
        {
            putchar(c);
        }
        printf("\"\n");
    }
    else
    {
        printf("usage: S <address>\n");
    }
}

static void
cmd_dump_range(char *args)
{
    uint32_t start, end;

    if (sscanf(args, "%x %x", &start, &end) == 2)
    {
        for (uint32_t addr = start; addr <= end; addr += 4)
        {
            printf("0x%08x: 0x%08x\n", addr, mem_read32_unsigned(addr));
        }
    }
    else
    {
        printf("usage: d <start_address> <end_address>\n");
    }
}

static void
cmd_disasm(char *args)
{
    uint32_t addr;
    int count = 1;

    char *token = strtok(args, " \t\n");
    if (!token)
    {
        addr = g_state.pc;
    }
    else if (sscanf(token, "%x", &addr) != 1)
    {
        printf("usage: u <address> [count]\n");
        return;
    }

    token = strtok(NULL, " \t\n");
    if (token)
    {
        sscanf(token, "%d", &count);
        if (count < 1) count = 1;
        if (count > 32) count = 32;
    }

    printf("Disassembly from 0x%08x (%d instruction%s):\n", addr, count,
           count > 1 ? "s" : "");
    for (int i = 0; i < count; i++)
    {
        uint32_t ins = mem_read32_unsigned(addr);
        char *disasm_str = disasm(ins);
        printf("0x%08x:  %08x  %s\n", addr, ins, disasm_str);
        addr += 4;
    }
}

static void
cmd_toggle_disasm(void)
{
    show_disasm = !show_disasm;
    printf("Disassembly display: %s\n", show_disasm ? "ON" : "OFF (raw hex)");
}

static void
cmd_breakpoint_set(char *args)
{
    uint32_t addr;

    if (sscanf(args, "%x", &addr) == 1)
    {
        g_state.breakpoint = addr;
        g_state.breakpoint_enabled = 1;
        printf("Breakpoint set at 0x%08x\n", addr);
    }
    else
    {
        printf("usage: b <address>\n");
    }
}

static void
cmd_breakpoint_clear(void)
{
    g_state.breakpoint_enabled = 0;
    printf("Breakpoint cleared\n");
}

static void
cmd_help(void)
{
    printf("RV32IM Debugger Commands:\n");
    printf("  s           - Single step\n");
    printf("  n           - Single step (alias)\n");
    printf("  c           - Continue execution\n");
    printf("  q           - Quit emulator\n");
    printf("  r           - Print registers\n");
    printf("  p <addr>    - Set Program Counter (PC)\n");
    printf("  R <reg> <v> - Set register value (reg: 0-31 or name)\n");
    printf("  m <addr>... - Display memory at address(es)\n");
    printf("  d <s> <e>   - Dump memory range\n");
    printf("  B <a> [c]   - Dump bytes from address (count default=16)\n");
    printf("  S <addr>    - Print string from address\n");
    printf("  w <a> <v>   - Write value(s) to memory\n");
    printf("  u <a> [c]   - Disassemble instructions (default: PC, count=1)\n");
    printf("  D           - Toggle disassembly/raw hex display\n");
    printf("  b <addr>    - Set breakpoint\n");
    printf("  C           - Clear breakpoint\n");
    printf("  h/?         - This help\n");
    printf("  <Enter>     - Repeat last command\n");
}

static void
cmd_set_pc(char *args)
{
    uint32_t addr;

    if (sscanf(args, "%x", &addr) == 1)
    {
        if (addr & 0x3)
        {
            printf("Warning: PC 0x%08x is not 4-byte aligned\n", addr);
        }
        g_state.pc = addr;
        printf("PC set to 0x%08x\n", g_state.pc);
    }
    else
    {
        printf("usage: p <address>\n");
    }
}

static void
cmd_set_register(char *args)
{
    const char *reg_names[]
        = { "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
            "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
            "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
            "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

    uint32_t value;
    int reg_num = -1;

    char *token = strtok(args, " \t\n");
    if (!token)
    {
        printf("usage: R <register> <value>\n");
        printf("  register: 0-31 or name (e.g., a0, sp, t0)\n");
        return;
    }

    if (sscanf(token, "%d", &reg_num) == 1)
    {
        if (reg_num < 0 || reg_num > 31)
        {
            printf("Error: register number must be 0-31\n");
            return;
        }
    }
    else
    {
        int found = 0;
        for (int i = 0; i < 32; i++)
        {
            if (strcasecmp(token, reg_names[i]) == 0)
            {
                reg_num = i;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            printf("Error: unknown register '%s'\n", token);
            return;
        }
    }

    token = strtok(NULL, " \t\n");
    if (!token)
    {
        printf("Error: value required\n");
        return;
    }

    if (sscanf(token, "%x", &value) != 1)
    {
        printf("Error: invalid value '%s'\n", token);
        return;
    }

    if (reg_num == 0)
    {
        printf("Warning: x0 (zero) register is read-only, value remains 0\n");
        return;
    }

    g_state.gpr[reg_num] = value;
    printf("x%d (%s) = 0x%08x\n", reg_num, reg_names[reg_num], value);
}

void
tick_debugger(void)
{
    uint32_t ins = mem_read32_unsigned(g_state.pc);

    if (show_disasm)
    {
        char *disasm_str = disasm(ins);
        printf("0x%08x:  %08x  %s\n", g_state.pc, ins, disasm_str);
    }
    else
    {
        printf("0x%08x: %08x\n", g_state.pc, ins);
    }

    while (1)
    {
        char line[256];
        printf("DEBUG> ");

        if (!fgets(line, sizeof(line), stdin))
        {
            break;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
        }

        char *p = line;
        while (*p && isspace(*p)) p++;

        if (!*p)
        {
            if (last_cmd[0] != '\0')
            {
                strcpy(line, last_cmd);
                p = line;
                while (*p && isspace(*p)) p++;
                if (!*p) continue;
            }
            else
            {
                continue;
            }
        }
        else
        {
            strcpy(last_cmd, line);
        }

        char cmd = *p++;
        char *args = p;

        while (*args && isspace(*args)) args++;

        switch (cmd)
        {
        case 's':
        case 'n':
            cmd_single_step();
            return;

        case 'c':
            cmd_continue();
            return;

        case 'q':
            cmd_quit();
            return;

        case 'r':
            cmd_registers();
            continue;

        case 'p':
            cmd_set_pc(args);
            continue;

        case 'R':
            cmd_set_register(args);
            continue;

        case 'm':
            cmd_memory(args);
            continue;

        case 'w':
            cmd_write(args);
            continue;

        case 'B':
            cmd_dump_bytes(args);
            continue;

        case 'S':
            cmd_string(args);
            continue;

        case 'd':
            cmd_dump_range(args);
            continue;

        case 'u':
            cmd_disasm(args);
            continue;

        case 'D':
            cmd_toggle_disasm();
            continue;

        case 'b':
            cmd_breakpoint_set(args);
            continue;

        case 'C':
            cmd_breakpoint_clear();
            continue;

        case 'h':
        case '?':
            cmd_help();
            continue;

        default:
            if (cmd != '\n' && !isspace(cmd))
            {
                printf("unknown command: %c (type 'h' for help)\n", cmd);
            }
            break;
        }
    }
}

#endif // CONFIG_ENABLE_DEBUGGER
