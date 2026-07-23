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

#ifndef OPS_H
#define OPS_H

#include <emu.h>
#include <logger.h>
#include <mem.h>
#include <stdint.h>

#include <exec.h>

#define PC_BACKWARD g_state.pc -= 4 // in main loop we always pc+=4

static inline void
insi_r_add(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 + v2);
}

static inline void
insi_r_sub(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 - v2);
}

static inline void
insi_r_xor(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 ^ v2);
}

static inline void
insi_r_or(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 | v2);
}

static inline void
insi_r_and(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 & v2);
}

static inline void
insi_r_sll(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F;
    reg_write(rd, v1 << v2);
}

static inline void
insi_r_srl(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F;
    reg_write(rd, v1 >> v2);
}

static inline void
insi_r_sra(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F;
    reg_write(rd, (uint32_t)(v1 >> v2));
}

static inline void
insi_r_slt(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

static inline void
insi_r_sltu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

static inline void
insi_i_addi(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 + imm);
}

static inline void
insi_i_xori(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 ^ imm);
}

static inline void
insi_i_ori(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 | imm);
}

static inline void
insi_i_andi(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 & imm);
}

static inline void
insi_i_slli(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t shamt = imm & 0x1F;
    reg_write(rd, v1 << shamt);
}

static inline void
insi_i_srli(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t shamt = imm & 0x1F;
    reg_write(rd, v1 >> shamt);
}

static inline void
insi_i_srai(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t shamt = imm & 0x1F;
    reg_write(rd, (uint32_t)(v1 >> shamt));
}

static inline void
insi_i_slti(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)imm;
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

static inline void
insi_i_sltiu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = imm;
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

static inline void
insi_i_lb(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    int32_t val = mem_read8_signed(addr);
    reg_write(rd, (uint32_t)val);
}

static inline void
insi_i_lh(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(addr & 0x1))
    {
        fatal("LH misaligned address: 0x%x", addr);
    }
    int32_t val = mem_read16_signed(addr);
    reg_write(rd, (uint32_t)val);
}

static inline void
insi_i_lw(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(addr & 0x3))
    {
        fatal("LW misaligned address: 0x%x", addr);
    }
    int32_t val = mem_read32_signed(addr);
    reg_write(rd, (uint32_t)val);
}

static inline void
insi_i_lbu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val = mem_read8_unsigned(addr);
    reg_write(rd, val);
}

static inline void
insi_i_lhu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(addr & 0x1))
    {
        fatal("LHU misaligned address: 0x%x", addr);
    }
    uint32_t val = mem_read16_unsigned(addr);
    reg_write(rd, val);
}

static inline void
insi_i_jalr(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    addr = addr & ~1;

    reg_write(rd, g_state.pc + 4);
    g_state.pc = addr;
    PC_BACKWARD;
}

static inline void
insi_s_sb(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val = reg_read(rs2);
    mem_write8(addr, (uint8_t)(val & 0xFF));
}

static inline void
insi_s_sh(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(addr & 0x1))
    {
        fatal("SH misaligned address: 0x%x", addr);
    }
    uint32_t val = reg_read(rs2);
    mem_write16(addr, (uint16_t)(val & 0xFFFF));
}

static inline void
insi_s_sw(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(addr & 0x3))
    {
        fatal("SW misaligned address: 0x%x", addr);
    }
    uint32_t val = reg_read(rs2);
    mem_write32(addr, val);
}

static inline void
insi_b_beq(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (likely(v1 == v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_b_bne(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (likely(v1 != v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_b_blt(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    if (likely(v1 < v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_b_bge(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    if (likely(v1 >= v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_b_bltu(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (likely(v1 < v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_b_bgeu(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (likely(v1 >= v2))
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

static inline void
insi_u_lui(uint32_t imm, uint32_t rd)
{
    reg_write(rd, imm);
}

static inline void
insi_u_auipc(uint32_t imm, uint32_t rd)
{
    reg_write(rd, g_state.pc + imm);
}

static inline void
insi_j_jal(uint32_t imm, uint32_t rd)
{
    reg_write(rd, g_state.pc + 4);
    g_state.pc += imm;
    PC_BACKWARD;
}

static inline void
insi_i_ecall(void)
{
    info("ECALL instruction executed, Terminating...");
    g_state.terminated = 1;
}

static inline void
insi_i_ebreak(void)
{
    info("Stopped at EBREAK");
    g_state.single_step = 1;
}

static inline void
ins_fence(void)
{
    // do nothing
}

static inline uint32_t
sign_extend_12(uint32_t imm)
{
    if (unlikely(imm & 0x800)) return imm | 0xFFFFF000;
    return imm;
}

static inline uint32_t
sign_extend_13(uint32_t imm)
{
    if (unlikely(imm & 0x1000)) return imm | 0xFFFFE000;
    return imm;
}

static inline uint32_t
sign_extend_21(uint32_t imm)
{
    if (unlikely(imm & 0x100000)) return imm | 0xFFE00000;
    return imm;
}

#endif // OPS_H
