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

#include <stdint.h>

#include <config.h>

#ifdef CONFIG_ENABLE_C_EXTENSION

#include <basic_instructions.h>
#include <emu.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
#include <extension/system.h>
#endif

/* ---------------------------------------------------------------------------
 * RVC immediate field helpers.
 *
 * The compressed immediate fields are scattered across the 16-bit word and
 * must be reassembled into their semantic position before sign/zero
 * extension. The bit mappings below follow the ratified RISC-V C extension
 * (they were cross-checked field-by-field against the GNU binutils
 * disassembler for rv32gc).
 * ------------------------------------------------------------------------- */

static inline int32_t
sext6(uint32_t v)
{
    v &= 0x3F;
    return (v >= 0x20) ? (int32_t)(v - 0x40) : (int32_t)v;
}

static inline int32_t
sext10(uint32_t v)
{
    v &= 0x3FF;
    return (v >= 0x200) ? (int32_t)(v - 0x400) : (int32_t)v;
}

/* c.addi4spn: nzuimm[9:2]. */
static inline uint32_t
c_addi4spn_imm(uint16_t c)
{
    return (((uint32_t)((c >> 11) & 0x3) << 4)    // nzuimm[5:4] = c[12:11]
            | ((uint32_t)((c >> 7) & 0xF) << 6)   // nzuimm[9:6] = c[10:7]
            | ((uint32_t)((c >> 6) & 0x1) << 2)   // nzuimm[2]   = c[6]
            | ((uint32_t)((c >> 5) & 0x1) << 3)); // nzuimm[3] = c[5]
}

/* c.lw / c.sw: uimm[6:2]. */
static inline uint32_t
c_ls_uimm(uint16_t c)
{
    return (((uint32_t)((c >> 5) & 0x1) << 6)     // uimm[6]   = c[5]
            | ((uint32_t)((c >> 10) & 0x7) << 3)  // uimm[5:3] = c[12:10]
            | ((uint32_t)((c >> 6) & 0x1) << 2)); // uimm[2]   = c[6]
}

/* c.lwsp: uimm[7:2]. */
static inline uint32_t
c_lwsp_uimm(uint16_t c)
{
    return (((uint32_t)((c >> 2) & 0x3) << 6)     // uimm[7:6] = c[3:2]
            | ((uint32_t)((c >> 12) & 0x1) << 5)  // uimm[5]   = c[12]
            | ((uint32_t)((c >> 4) & 0x7) << 2)); // uimm[4:2] = c[6:4]
}

/* c.swsp: uimm[7:2]. */
static inline uint32_t
c_swsp_uimm(uint16_t c)
{
    return (((uint32_t)((c >> 7) & 0x3) << 6)     // uimm[7:6] = c[8:7]
            | ((uint32_t)((c >> 9) & 0xF) << 2)); // uimm[5:2] = c[12:9]
}

/* c.addi16sp: nzimm[9:0]. */
static inline int32_t
c_addi16sp_imm(uint16_t c)
{
    return sext10(((uint32_t)((c >> 12) & 0x1) << 9)    // nzimm[9] = c[12]
                  | ((uint32_t)((c >> 4) & 0x1) << 8)   // nzimm[8] = c[4]
                  | ((uint32_t)((c >> 3) & 0x1) << 7)   // nzimm[7] = c[3]
                  | ((uint32_t)((c >> 5) & 0x1) << 6)   // nzimm[6] = c[5]
                  | ((uint32_t)((c >> 2) & 0x1) << 5)   // nzimm[5] = c[2]
                  | ((uint32_t)((c >> 6) & 0x1) << 4)); // nzimm[4] = c[6]
}

/* c.j / c.jal: 12-bit signed PC offset. */
static inline int32_t
c_jal_offset(uint16_t c)
{
    uint32_t imm = (((uint32_t)((c >> 12) & 0x1) << 11)   // imm[11] = c[12]
                    | ((uint32_t)((c >> 11) & 0x1) << 4)  // imm[4]  = c[11]
                    | ((uint32_t)((c >> 9) & 0x3) << 8)   // imm[9:8]= c[10:9]
                    | ((uint32_t)((c >> 8) & 0x1) << 10)  // imm[10] = c[8]
                    | ((uint32_t)((c >> 7) & 0x1) << 6)   // imm[6]  = c[7]
                    | ((uint32_t)((c >> 6) & 0x1) << 7)   // imm[7]  = c[6]
                    | ((uint32_t)((c >> 2) & 0x1) << 5)   // imm[5]  = c[2]
                    | ((uint32_t)((c >> 3) & 0x7) << 1)); // imm[3:1]= c[5:3]
    return (imm & 0x800) ? (int32_t)(imm - 0x1000) : (int32_t)imm;
}

/* c.beqz / c.bnez: 9-bit signed PC offset. */
static inline int32_t
c_branch_offset(uint16_t c)
{
    uint32_t imm = (((uint32_t)((c >> 12) & 0x1) << 8)    // imm[8] = c[12]
                    | ((uint32_t)((c >> 6) & 0x1) << 7)   // imm[7] = c[6]
                    | ((uint32_t)((c >> 5) & 0x1) << 6)   // imm[6] = c[5]
                    | ((uint32_t)((c >> 2) & 0x1) << 5)   // imm[5] = c[2]
                    | ((uint32_t)((c >> 11) & 0x1) << 4)  // imm[4] = c[11]
                    | ((uint32_t)((c >> 10) & 0x1) << 3)  // imm[3] = c[10]
                    | ((uint32_t)((c >> 4) & 0x1) << 2)   // imm[2] = c[4]
                    | ((uint32_t)((c >> 3) & 0x1) << 1)); // imm[1] = c[3]
    return (imm & 0x100) ? (int32_t)(imm - 0x200) : (int32_t)imm;
}

static inline uint32_t
c_lui_imm(uint16_t c)
{
    return (uint32_t)sext6(((uint32_t)((c >> 12) & 0x1) << 5)
                           | ((c >> 2) & 0x1F))
           << 12;
}

static inline int32_t
c_imm6(uint16_t c)
{
    return sext6(((uint32_t)((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1F));
}

static inline uint32_t
c_shamt(uint16_t c)
{
    return (((uint32_t)((c >> 12) & 0x1) << 5) | ((c >> 2) & 0x1F));
}

/* ---------------------------------------------------------------------------
 * exec_c_insn
 *
 * Decode and execute a single 16-bit compressed instruction.
 *
 * PC handling (mirrors the main loop's "exec then pc += 4" convention):
 *   - sequential (non-control-flow) instructions: we set g_state.pc -= 2 so the
 *     loop's += 4 advances exactly 16 bits to the next instruction;
 *   - control-flow (jumps/branches taken): we set g_state.pc = <target> and
 * then subtract 4 (PC_BACKWARD) so the loop's += 4 lands on <target>;
 *   - traps (illegal instruction / misaligned fetch): raise_exception()/the
 *     caller already repositioned PC, so we return 1 and never touch PC here.
 *
 * Returns 1 if a trap was raised, 0 otherwise.
 * ------------------------------------------------------------------------- */
int
exec_c_insn(uint16_t c)
{
    uint32_t q = c & 0x3;
    uint32_t f3 = (c >> 13) & 0x7;

    if (q == 0) // ---------------- Quadrant 0 ----------------
    {
        uint32_t rs1p = 8 + ((c >> 7) & 0x7);
        uint32_t rdp = 8 + ((c >> 2) & 0x7);
        uint32_t rs2p = 8 + ((c >> 2) & 0x7);

        switch (f3)
        {
        case 0: // c.addi4spn
        {
            uint32_t nzuimm = c_addi4spn_imm(c);
            if (nzuimm == 0) // reserved encoding
            {
                raise_illegal(c);
                return 1;
            }
            reg_write(rdp, reg_read(2) + nzuimm);
            g_state.pc -= 2;
            return 0;
        }
        case 1: // c.fld (rv32 F/D) — not required by rv32uc
        case 3: // c.ld / c.flw — not required by rv32uc
            raise_illegal(c);
            return 1;
        case 2: // c.lw
            insi_i_lw((int32_t)c_ls_uimm(c), rs1p, rdp);
            g_state.pc -= 2;
            return 0;
        case 6: // c.sw
            insi_s_sw((int32_t)c_ls_uimm(c), rs2p, rs1p);
            g_state.pc -= 2;
            return 0;
        default:
            break;
        }
    }
    else if (q == 1) // ---------------- Quadrant 1 ----------------
    {

        uint32_t rd = (c >> 7) & 0x1F;
        switch (f3)
        {
        case 0: // c.addi / c.nop
            reg_write(rd, reg_read(rd) + (uint32_t)c_imm6(c));
            g_state.pc -= 2;
            return 0;
        case 1: // c.jal (RV32)
            reg_write(1, g_state.pc + 2);
            g_state.pc += c_jal_offset(c);
            PC_BACKWARD;
            return 0;
        case 2: // c.li
            reg_write(rd, (uint32_t)c_imm6(c));
            g_state.pc -= 2;
            return 0;
        case 3: // c.addi16sp (rd==x2) / c.lui
            if (rd == 2)
            {
                reg_write(2, reg_read(2) + (uint32_t)c_addi16sp_imm(c));
            }
            else if (rd != 0) // rd==x0 is a reserved hint
            {
                reg_write(rd, c_lui_imm(c));
            }
            else
            {
                raise_illegal(c);
                return 1;
            }
            g_state.pc -= 2;
            return 0;
        case 4: // Q1 ALU + shift group
        {
            uint32_t funct2 = (c >> 10) & 0x3; // c[11:10]
            uint32_t rdp = 8 + ((c >> 7) & 0x7);
            uint32_t rs2p = 8 + ((c >> 2) & 0x7);
            if (funct2 <= 1) // c.srli (0) / c.srai (1)
            {
                uint32_t shamt = c_shamt(c);
                uint32_t val = reg_read(rdp);
                if (shamt == 0) // reserved on RV32
                {
                    raise_illegal(c);
                    return 1;
                }
                if (funct2 == 0)
                {
                    reg_write(rdp, val >> (shamt & 0x1F));
                }
                else
                {
                    reg_write(rdp, (uint32_t)((int32_t)val >> (shamt & 0x1F)));
                }
            }
            else if (funct2 == 2) // c.andi
            {
                reg_write(rdp, reg_read(rdp) & (uint32_t)c_imm6(c));
            }
            else // funct2 == 3: c.sub / c.xor / c.or / c.and
            {
                uint32_t alu = (c >> 5) & 0x3; // c[6:5]
                uint32_t rs1v = reg_read(rdp);
                uint32_t rs2v = reg_read(rs2p);
                if (alu == 0)
                    reg_write(rdp, rs1v - rs2v); // c.sub
                else if (alu == 1)
                    reg_write(rdp, rs1v ^ rs2v); // c.xor
                else if (alu == 2)
                    reg_write(rdp, rs1v | rs2v); // c.or
                else
                    reg_write(rdp, rs1v & rs2v); // c.and
            }
            g_state.pc -= 2;
            return 0;
        }
        case 5: // c.j
            g_state.pc += c_jal_offset(c);
            PC_BACKWARD;
            return 0;
        case 6: // c.beqz
        case 7: // c.bnez
        {
            uint32_t rs1p = 8 + ((c >> 7) & 0x7);
            uint32_t v = reg_read(rs1p);
            int32_t off = c_branch_offset(c);
            if ((f3 == 6 && v == 0) || (f3 == 7 && v != 0))
            {
                g_state.pc += off;
                PC_BACKWARD;
            }
            else
            {
                g_state.pc -= 2;
            }
            return 0;
        }
        default:
            break;
        }
    }
    else // ------------- Quadrant 2 (q == 2) -------------
    {

        uint32_t rd = (c >> 7) & 0x1F;
        switch (f3)
        {
        case 0: // c.slli
        {
            uint32_t shamt = c_shamt(c);
            if (shamt == 0) // reserved on RV32
            {
                raise_illegal(c);
                return 1;
            }
            reg_write(rd, reg_read(rd) << (shamt & 0x1F));
            g_state.pc -= 2;
            return 0;
        }
        case 2:          // c.lwsp
            if (rd == 0) // reserved
            {
                raise_illegal(c);
                return 1;
            }
            insi_i_lw((int32_t)c_lwsp_uimm(c), 2, rd);
            g_state.pc -= 2;
            return 0;
        case 4: // c.jr / c.mv / c.ebreak / c.jalr / c.add
        {
            uint32_t rs1 = (c >> 7) & 0x1F;
            uint32_t rs2 = (c >> 2) & 0x1F;
            if (((c >> 12) & 0x1) == 0) // c.jr / c.mv
            {
                if (rs2 == 0) // c.jr rs1
                {
                    if (rs1 == 0) // reserved hint, treated as NOP
                    {
                        g_state.pc -= 2;
                        return 0;
                    }
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
                    if (raise_if_misaligned_fetch(reg_read(rs1)))
                    {
                        return 1;
                    }
#endif
                    g_state.pc = reg_read(rs1);
                    PC_BACKWARD;
                }
                else // c.mv rd, rs2
                {
                    reg_write(rs1, reg_read(rs2));
                    g_state.pc -= 2;
                }
            }
            else // bit12 set: c.ebreak / c.jalr / c.add
            {
                if (rs1 == 0) // c.ebreak
                {
                    raise_exception(CAUSE_BREAKPOINT, 0);
                    return 1;
                }
                if (rs2 == 0) // c.jalr rs1 (writes x1)
                {
#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
                    if (raise_if_misaligned_fetch(reg_read(rs1)))
                    {
                        return 1;
                    }
#endif
                    reg_write(1, g_state.pc + 2);
                    g_state.pc = reg_read(rs1);
                    PC_BACKWARD;
                }
                else // c.add rd, rs2
                {
                    reg_write(rs1, reg_read(rs1) + reg_read(rs2));
                    g_state.pc -= 2;
                }
            }
            return 0;
        }
        case 6: // c.swsp
            insi_s_sw((int32_t)c_swsp_uimm(c), (c >> 2) & 0x1F, 2);
            g_state.pc -= 2;
            return 0;
        default:
            break;
        }
    }

    // Unreachable / unallocated encoding.
    raise_illegal(c);
    return 1;
}

#endif // CONFIG_ENABLE_C_EXTENSION
