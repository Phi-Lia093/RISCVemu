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

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <extension/f_extension.h>
#endif

#ifdef CONFIG_ENABLE_DEBUGGER

static char last_cmd[256] = { 0 };

static int show_disasm = 1; // 1 = show disassembly, 0 = show raw hex

static const char *reg_names[]
    = { "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

#ifdef CONFIG_ENABLE_F_EXTENSION
static const char *freg_names[]
    = { "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
        "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
        "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
        "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31" };
#endif

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

#ifdef CONFIG_ENABLE_F_EXTENSION
static void cmd_fregisters(void);
static void cmd_set_fregister(char *args);
static void cmd_fcsr(void);
static void cmd_set_fcsr(char *args);
static void cmd_fdump(char *args);
static void cmd_frm(void);
#endif

void
init_debugger(void)
{
    last_cmd[0] = '\0';
    show_disasm = 1;
}

static void
print_registers(void)
{
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
    printf("RV32IMAFDQ Debugger Commands:\n");
    printf("  s           - Single step\n");
    printf("  n           - Single step (alias)\n");
    printf("  c           - Continue execution\n");
    printf("  q           - Quit emulator\n");
    printf("  r           - Print integer registers\n");
    printf("  p <addr>    - Set Program Counter (PC)\n");
    printf("  R <reg> <v> - Set integer register value (reg: 0-31 or name)\n");
    printf("  m <addr>... - Display memory at address(es)\n");
    printf("  d <s> <e>   - Dump memory range\n");
    printf("  B <a> [c]   - Dump bytes from address (count default=16)\n");
    printf("  S <addr>    - Print string from address\n");
    printf("  w <a> <v>   - Write value(s) to memory\n");
    printf("  u <a> [c]   - Disassemble instructions (default: PC, count=1)\n");
    printf("  D           - Toggle disassembly/raw hex display\n");
    printf("  b <addr>    - Set breakpoint\n");
    printf("  C           - Clear breakpoint\n");
#ifdef CONFIG_ENABLE_F_EXTENSION
    printf("  f           - Show all FP registers\n");
    printf("  F <freg>    - Show specific FP register (F f0, F f1, etc.)\n");
    printf("  F <f> <v>   - Set FP register (F f0 0x3f800000 or F f0 1.0)\n");
    printf("  fc          - Show FCSR register\n");
    printf("  fc <v>      - Set FCSR register (hex)\n");
    printf("  frm         - Show current rounding mode\n");
    printf("  fd <a> [f]  - Dump memory as float (f/d/q, default=f)\n");
#endif
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

#ifdef CONFIG_ENABLE_F_EXTENSION

static void
cmd_fregisters(void)
{
    printf("FP REGISTERS (FPR):\n");

    // Single precision
    printf("  Single-precision (hex / float):\n");
    for (int i = 0; i < 32; i++)
    {
        uint32_t val = fpr_read_s(i);
        float fval;
        memcpy(&fval, &val, sizeof(float));
        printf("  %s=0x%08x (%-12.6g)", freg_names[i], val, (double)fval);
        if ((i + 1) % 2 == 0) printf("\n");
    }
    printf("\n");

#ifdef CONFIG_ENABLE_D_EXTENSION
    printf("  Double-precision (hex / double):\n");
    for (int i = 0; i < 32; i++)
    {
        uint64_t val = fpr_read_d(i);
        double dval;
        memcpy(&dval, &val, sizeof(double));
        printf("  %s=0x%016llx (%-12.6g)", freg_names[i],
               (unsigned long long)val, dval);
        if ((i + 1) % 2 == 0) printf("\n");
    }
    printf("\n");
#endif

#ifdef CONFIG_ENABLE_Q_EXTENSION
    printf("  Quad-precision (hex, low/high):\n");
    for (int i = 0; i < 32; i++)
    {
        float128_t q;
        fpr_read_q(i, &q);
        printf("  %s=0x%016llx_%016llx", freg_names[i],
               (unsigned long long)q.v[1], (unsigned long long)q.v[0]);
        if ((i + 1) % 2 == 0) printf("\n");
    }
    printf("\n");
#endif
}

static void
cmd_fdump(char *args)
{
    uint32_t addr;
    char format = 'f'; // 'f' for float, 'd' for double, 'q' for quad

    char *token = strtok(args, " \t\n");
    if (!token)
    {
        printf("usage: fd <address> [f|d|q]\n");
        printf("  f - dump as float (default)\n");
        printf("  d - dump as double\n");
        printf("  q - dump as quad\n");
        return;
    }

    if (sscanf(token, "%x", &addr) != 1)
    {
        printf("invalid address: %s\n", token);
        return;
    }

    token = strtok(NULL, " \t\n");
    if (token)
    {
        format = tolower(token[0]);
    }

    switch (format)
    {
    case 'f':
    {
        uint32_t val = mem_read32_unsigned(addr);
        float fval;
        memcpy(&fval, &val, sizeof(float));
        printf("0x%08x: 0x%08x = %f\n", addr, val, (double)fval);
        break;
    }
#ifdef CONFIG_ENABLE_D_EXTENSION
    case 'd':
    {
        uint64_t val;
#ifdef CONFIG_SUPPORT_MISALIGN
        if (unlikely(!is_aligned(addr, 8)))
        {
            val = 0;
            for (int i = 0; i < 8; i++)
                val |= (uint64_t)mem_read8_unsigned(addr + i) << (8 * i);
        }
        else
#endif
            val = mem_read64_unsigned(addr);
        double dval;
        memcpy(&dval, &val, sizeof(double));
        printf("0x%08x: 0x%016llx = %g\n", addr, (unsigned long long)val, dval);
        break;
    }
#endif
#ifdef CONFIG_ENABLE_Q_EXTENSION
    case 'q':
    {
        float128_t q;
#ifdef CONFIG_SUPPORT_MISALIGN
        if (unlikely(!is_aligned(addr, 16)))
        {
            q.v[0] = 0;
            q.v[1] = 0;
            for (int i = 0; i < 8; i++)
                q.v[0] |= (uint64_t)mem_read8_unsigned(addr + i) << (8 * i);
            for (int i = 0; i < 8; i++)
                q.v[1] |= (uint64_t)mem_read8_unsigned(addr + 8 + i) << (8 * i);
        }
        else
#endif
        {
            q.v[0] = mem_read64_unsigned(addr);
            q.v[1] = mem_read64_unsigned(addr + 8);
        }
        printf("0x%08x: 0x%016llx_%016llx\n", addr, (unsigned long long)q.v[1],
               (unsigned long long)q.v[0]);
        break;
    }
#endif
    default:
        printf("unknown format: %c (use f, d, or q)\n", format);
        break;
    }
}

static void
cmd_set_fregister(char *args)
{
    char *token = strtok(args, " \t\n");
    if (!token)
    {
        printf("usage: F <freg> [value]\n");
        printf("  F f0         - show f0 value\n");
        printf("  F f0 0x3f800000 - set f0 to 1.0 (single precision)\n");
        printf(
            "  F f0 0x3ff0000000000000 - set f0 to 1.0 (double precision)\n");
        printf("  F f0 1.0     - set f0 to 1.0\n");
        return;
    }

    // Parse register name
    int reg_num = -1;
    if (token[0] == 'f' || token[0] == 'F')
    {
        int num;
        if (sscanf(token + 1, "%d", &num) == 1 && num >= 0 && num <= 31)
        {
            reg_num = num;
        }
    }

    if (reg_num < 0)
    {
        // Try matching full name
        for (int i = 0; i < 32; i++)
        {
            if (strcasecmp(token, freg_names[i]) == 0)
            {
                reg_num = i;
                break;
            }
        }
    }

    if (reg_num < 0 || reg_num > 31)
    {
        printf("invalid FP register: %s (use f0-f31)\n", token);
        return;
    }

    token = strtok(NULL, " \t\n");
    if (!token)
    {
        // Show current value
        uint32_t val = fpr_read_s(reg_num);
        float fval;
        memcpy(&fval, &val, sizeof(float));
        printf("%s = 0x%08x (%f)\n", freg_names[reg_num], val, (double)fval);
#ifdef CONFIG_ENABLE_D_EXTENSION
        uint64_t dval = fpr_read_d(reg_num);
        double ddval;
        memcpy(&ddval, &dval, sizeof(double));
        printf("  as double: 0x%016llx (%g)\n", (unsigned long long)dval,
               ddval);
#endif
        return;
    }

    // Try parsing as hex first
    uint64_t value;
    if (sscanf(token, "%llx", (unsigned long long *)&value) == 1)
    {
        // If value fits in 32 bits, write as single precision
        if (value <= 0xFFFFFFFFULL)
        {
            fpr_write_s(reg_num, (uint32_t)value);
            float fval;
            memcpy(&fval, &value, sizeof(float));
            printf("%s set to 0x%08x (%f)\n", freg_names[reg_num],
                   (uint32_t)value, (double)fval);
        }
#ifdef CONFIG_ENABLE_D_EXTENSION
        else
        {
            fpr_write_d(reg_num, value);
            double dval;
            memcpy(&dval, &value, sizeof(double));
            printf("%s set to 0x%016llx (%g)\n", freg_names[reg_num],
                   (unsigned long long)value, dval);
        }
#endif
        return;
    }

    // Try parsing as decimal float
    double dval;
    if (sscanf(token, "%lf", &dval) == 1)
    {
        // Check if it's a float or double value
        float single = (float)dval;
        uint32_t single_bits;
        memcpy(&single_bits, &single, sizeof(float));
        fpr_write_s(reg_num, single_bits);
        printf("%s set to %f (0x%08x)\n", freg_names[reg_num], (double)single,
               single_bits);
        return;
    }

    printf("invalid value: %s (use hex or decimal)\n", token);
}

static void
cmd_fcsr(void)
{
    printf("FCSR = 0x%08x\n", fcsr);
    printf("  fflags   = 0x%02x (", get_fflags());
    uint32_t flags = get_fflags();
    if (flags & NV) printf("NV ");
    if (flags & DZ) printf("DZ ");
    if (flags & OF) printf("OF ");
    if (flags & UF) printf("UF ");
    if (flags & NX) printf("NX");
    if (flags == 0) printf("none");
    printf(")\n");
    printf("  frm      = 0x%02x (", get_frm());
    switch (get_frm())
    {
    case 0:
        printf("RNE - Round to Nearest, ties to Even)\n");
        break;
    case 1:
        printf("RTZ - Round towards Zero\n");
        break;
    case 2:
        printf("RDN - Round Down (towards -inf)\n");
        break;
    case 3:
        printf("RUP - Round Up (towards +inf)\n");
        break;
    case 4:
        printf("RMM - Round to Nearest, ties to Max Magnitude\n");
        break;
    default:
        printf("INVALID)\n");
        break;
    }
}

static void
cmd_set_fcsr(char *args)
{
    uint32_t value;
    if (sscanf(args, "%x", &value) != 1)
    {
        printf("usage: fc <value> (hex, e.g., fc 0x1f)\n");
        return;
    }
    set_fcsr(value);
    printf("FCSR set to 0x%08x\n", fcsr);
}

static void
cmd_frm(void)
{
    uint32_t frm = get_frm();
    printf("Current rounding mode: ");
    switch (frm)
    {
    case 0:
        printf("RNE (Round to Nearest, ties to Even)\n");
        break;
    case 1:
        printf("RTZ (Round towards Zero)\n");
        break;
    case 2:
        printf("RDN (Round Down, towards -inf)\n");
        break;
    case 3:
        printf("RUP (Round Up, towards +inf)\n");
        break;
    case 4:
        printf("RMM (Round to Nearest, ties to Max Magnitude)\n");
        break;
    default:
        printf("INVALID (%u)\n", frm);
        break;
    }
}

#endif // CONFIG_ENABLE_F_EXTENSION

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

        // Extract command and arguments
        char cmd_buf[16];
        char *args = p;
        int cmd_len = 0;
        while (*args && !isspace(*args) && cmd_len < 15)
        {
            cmd_buf[cmd_len++] = *args++;
        }
        cmd_buf[cmd_len] = '\0';
        while (*args && isspace(*args)) args++;

        // Handle commands
        if (strcmp(cmd_buf, "s") == 0 || strcmp(cmd_buf, "n") == 0)
        {
            cmd_single_step();
            return;
        }
        else if (strcmp(cmd_buf, "c") == 0)
        {
            cmd_continue();
            return;
        }
        else if (strcmp(cmd_buf, "q") == 0)
        {
            cmd_quit();
            return;
        }
        else if (strcmp(cmd_buf, "r") == 0)
        {
            cmd_registers();
            continue;
        }
        else if (strcmp(cmd_buf, "p") == 0)
        {
            cmd_set_pc(args);
            continue;
        }
        else if (strcmp(cmd_buf, "R") == 0)
        {
            cmd_set_register(args);
            continue;
        }
        else if (strcmp(cmd_buf, "m") == 0)
        {
            cmd_memory(args);
            continue;
        }
        else if (strcmp(cmd_buf, "w") == 0)
        {
            cmd_write(args);
            continue;
        }
        else if (strcmp(cmd_buf, "B") == 0)
        {
            cmd_dump_bytes(args);
            continue;
        }
        else if (strcmp(cmd_buf, "S") == 0)
        {
            cmd_string(args);
            continue;
        }
        else if (strcmp(cmd_buf, "d") == 0)
        {
            cmd_dump_range(args);
            continue;
        }
        else if (strcmp(cmd_buf, "u") == 0)
        {
            cmd_disasm(args);
            continue;
        }
        else if (strcmp(cmd_buf, "D") == 0)
        {
            cmd_toggle_disasm();
            continue;
        }
        else if (strcmp(cmd_buf, "b") == 0)
        {
            cmd_breakpoint_set(args);
            continue;
        }
        else if (strcmp(cmd_buf, "C") == 0)
        {
            cmd_breakpoint_clear();
            continue;
        }
#ifdef CONFIG_ENABLE_F_EXTENSION
        else if (strcmp(cmd_buf, "f") == 0)
        {
            cmd_fregisters();
            continue;
        }
        else if (strcmp(cmd_buf, "F") == 0)
        {
            cmd_set_fregister(args);
            continue;
        }
        else if (strcmp(cmd_buf, "fc") == 0)
        {
            if (*args)
                cmd_set_fcsr(args);
            else
                cmd_fcsr();
            continue;
        }
        else if (strcmp(cmd_buf, "fd") == 0)
        {
            cmd_fdump(args);
            continue;
        }
        else if (strcmp(cmd_buf, "frm") == 0)
        {
            cmd_frm();
            continue;
        }
#endif
        else if (strcmp(cmd_buf, "h") == 0 || strcmp(cmd_buf, "?") == 0)
        {
            cmd_help();
            continue;
        }
        else
        {
            printf("unknown command: %s (type 'h' for help)\n", cmd_buf);
            continue;
        }
    }
}

#endif // CONFIG_ENABLE_DEBUGGER
