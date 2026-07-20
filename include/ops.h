#ifndef OPS_H
#define OPS_H

#include <emu.h>
#include <logger.h>
#include <mem.h>
#include <stdint.h>

#include <exec.h>

#define PC_BACKWARD g_state.pc -= 4 // in main loop we always pc+=4

// ==================== Likely/Unlikely Macros ====================

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// ==================== R-type Integer Instructions ====================

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

// ==================== M-type Integer Instructions (NOT inlined - jump table)
// ====================

void insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd);

// ==================== I-type ALU Instructions ====================

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

// ==================== I-type Load Instructions ====================

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
    if (unlikely(!is_aligned(addr, 2)))
    {
        error("misaligned load halfword");
    }
    int32_t val = mem_read16_signed(addr);
    reg_write(rd, (uint32_t)val);
}

static inline void
insi_i_lw(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(!is_aligned(addr, 4)))
    {
        error("misaligned load word");
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
    if (unlikely(!is_aligned(addr, 2)))
    {
        error("misaligned load halfword unsigned");
    }
    uint32_t val = mem_read16_unsigned(addr);
    reg_write(rd, val);
}

// ==================== JALR Instruction ====================

static inline void
insi_i_jalr(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    addr = addr & ~1;

    reg_write(rd, g_state.pc + 4);
    g_state.pc = addr;
    PC_BACKWARD;
}

// ==================== S-type Store Instructions ====================

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
    if (unlikely(!is_aligned(addr, 2)))
    {
        error("misaligned store halfword");
    }
    uint32_t val = reg_read(rs2);
    mem_write16(addr, (uint16_t)(val & 0xFFFF));
}

static inline void
insi_s_sw(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (unlikely(!is_aligned(addr, 4)))
    {
        error("misaligned store word");
    }
    uint32_t val = reg_read(rs2);
    mem_write32(addr, val);
}

// ==================== B-type Branch Instructions ====================

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

// ==================== U-type Instructions ====================

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

// ==================== J-type Instruction ====================

static inline void
insi_j_jal(uint32_t imm, uint32_t rd)
{
    reg_write(rd, g_state.pc + 4);
    g_state.pc += imm;
    PC_BACKWARD;
}

// ==================== System Instructions ====================

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

// ==================== Sign Extension Helpers ====================

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
