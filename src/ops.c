#include <emu.h>
#include <exec.h>
#include <logger.h>
#include <mem.h>
#include <ops.h>
#include <stdint.h>

#define PC_BACKWARD g_state.pc -= 4 // in main loop we always pc+=4

// ==================== R-type Integer Instructions ====================

void
insi_r_add(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 + v2);
}

void
insi_r_sub(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 - v2);
}

void
insi_r_xor(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 ^ v2);
}

void
insi_r_or(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 | v2);
}

void
insi_r_and(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, v1 & v2);
}

void
insi_r_sll(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F; // Only lower 5 bits
    reg_write(rd, v1 << v2);
}

void
insi_r_srl(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F; // Only lower 5 bits
    reg_write(rd, v1 >> v2);
}

void
insi_r_sra(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t v2 = reg_read(rs2) & 0x1F; // Only lower 5 bits
    reg_write(rd, (uint32_t)(v1 >> v2));
}

void
insi_r_slt(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

void
insi_r_sltu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

// ==================== M-type Integer Instructions ====================

void
insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    reg_write(rd, (uint32_t)(v1 * v2));
}

void
insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    int64_t result = (int64_t)v1 * (int64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    int64_t result = (int64_t)v1 * (int64_t)(int32_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    uint64_t result = (uint64_t)v1 * (uint64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (v2 == 0)
    {
        reg_write(rd,
                  0xFFFFFFFF); // Undefined, but many implementations return -1
    }
    else if (v1 == INT32_MIN && v2 == -1)
    {
        reg_write(rd, (uint32_t)INT32_MIN); // Overflow case
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 / v2));
    }
}

void
insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (v2 == 0)
    {
        reg_write(rd, 0xFFFFFFFF); // Undefined
    }
    else
    {
        reg_write(rd, v1 / v2);
    }
}

void
insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (v2 == 0)
    {
        reg_write(rd,
                  v1); // Undefined, but many implementations return dividend
    }
    else if (v1 == INT32_MIN && v2 == -1)
    {
        reg_write(rd, 0); // Overflow case
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 % v2));
    }
}

void
insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (v2 == 0)
    {
        reg_write(rd, v1); // Undefined
    }
    else
    {
        reg_write(rd, v1 % v2);
    }
}

// ==================== I-type ALU Instructions ====================

void
insi_i_addi(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    // imm is already sign-extended by the caller
    reg_write(rd, v1 + imm);
}

void
insi_i_xori(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 ^ imm);
}

void
insi_i_ori(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 | imm);
}

void
insi_i_andi(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    reg_write(rd, v1 & imm);
}

void
insi_i_slli(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t shamt = imm & 0x1F; // Only lower 5 bits
    reg_write(rd, v1 << shamt);
}

void
insi_i_srli(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t shamt = imm & 0x1F; // Only lower 5 bits
    reg_write(rd, v1 >> shamt);
}

void
insi_i_srai(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t shamt = imm & 0x1F; // Only lower 5 bits
    reg_write(rd, (uint32_t)(v1 >> shamt));
}

void
insi_i_slti(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)imm; // imm is already sign-extended
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

void
insi_i_sltiu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    // imm is already sign-extended, but for unsigned comparison we treat it as
    // unsigned
    uint32_t v2 = imm;
    reg_write(rd, (v1 < v2) ? 1 : 0);
}

// ==================== I-type Load Instructions ====================

void
insi_i_lb(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    int32_t val = mem_read8_signed(addr);
    reg_write(rd, (uint32_t)val);
}

void
insi_i_lh(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (!is_aligned(addr, 2))
    {
        error("misaligned load halfword");
    }
    int32_t val = mem_read16_signed(addr);
    reg_write(rd, (uint32_t)val);
}

void
insi_i_lw(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (!is_aligned(addr, 4))
    {
        error("misaligned load word");
    }
    int32_t val = mem_read32_signed(addr);
    reg_write(rd, (uint32_t)val);
}

void
insi_i_lbu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val = mem_read8_unsigned(addr);
    reg_write(rd, val);
}

void
insi_i_lhu(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (!is_aligned(addr, 2))
    {
        error("misaligned load halfword unsigned");
    }
    uint32_t val = mem_read16_unsigned(addr);
    reg_write(rd, val);
}

// ==================== JALR Instruction ====================

void
insi_i_jalr(uint32_t imm, uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1) + imm;
    // Clear the lowest bit (must be aligned to 2 bytes)
    addr = addr & ~1;

    // Save return address (PC + 4) - PC is stored in g_state.pc
    reg_write(rd, g_state.pc + 4);

    // Jump to target address
    g_state.pc = addr;
    PC_BACKWARD;
}

// ==================== S-type Store Instructions ====================

void
insi_s_sb(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    uint32_t val = reg_read(rs2);
    mem_write8(addr, (uint8_t)(val & 0xFF));
}

void
insi_s_sh(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (!is_aligned(addr, 2))
    {
        error("misaligned store halfword");
    }
    uint32_t val = reg_read(rs2);
    mem_write16(addr, (uint16_t)(val & 0xFFFF));
}

void
insi_s_sw(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t addr = reg_read(rs1) + imm;
    if (!is_aligned(addr, 4))
    {
        error("misaligned store word");
    }
    uint32_t val = reg_read(rs2);
    mem_write32(addr, val);
}

// ==================== B-type Branch Instructions ====================

void
insi_b_beq(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (v1 == v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

void
insi_b_bne(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (v1 != v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

void
insi_b_blt(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    if (v1 < v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

void
insi_b_bge(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    if (v1 >= v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

void
insi_b_bltu(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (v1 < v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

void
insi_b_bgeu(uint32_t imm, uint32_t rs2, uint32_t rs1)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    if (v1 >= v2)
    {
        g_state.pc += imm;
        PC_BACKWARD;
    }
}

// ==================== U-type Instructions ====================

void
insi_u_lui(uint32_t imm, uint32_t rd)
{
    // imm is already aligned to 4KB (lower 12 bits are zero)
    reg_write(rd, imm);
}

void
insi_u_auipc(uint32_t imm, uint32_t rd)
{
    // imm is already aligned to 4KB (lower 12 bits are zero)
    // PC is stored in g_state.pc
    reg_write(rd, g_state.pc + imm);
}

// ==================== J-type Instruction ====================

void
insi_j_jal(uint32_t imm, uint32_t rd)
{
    // Save return address (PC + 4)
    reg_write(rd, g_state.pc + 4);

    // Jump to target address
    g_state.pc += imm;
    PC_BACKWARD;
}

// ==================== System Instructions ====================

void
insi_i_ecall(void)
{
    // Environment call - used for system calls
    // In a simple emulator, we can just print a message and halt
    info("ECALL instruction executed, Terminating...");
    g_state.terminated = 1;
}

void
insi_i_ebreak(void)
{
    // Environment break - used for debugging
    info("Stopped at EBREAK");
    g_state.single_step = 1;
}
