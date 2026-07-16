#ifndef OPS_H
#define OPS_H

#include <stdint.h>

// (rs2, rs1, rd)
void insi_r_add(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_sub(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_xor(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_or(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_and(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_sll(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_srl(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_sra(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_slt(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insi_r_sltu(uint32_t rs2, uint32_t rs1, uint32_t rd);

// (rs2, rs1, rd)
void insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd);

// (imm, rs1, rd)
void insi_i_addi(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_slti(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_sltiu(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_xori(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_ori(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_andi(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_slli(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_srli(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_srai(uint32_t imm, uint32_t rs1, uint32_t rd);

// (imm, rs1, rd)
void insi_i_lb(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_lh(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_lw(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_lbu(uint32_t imm, uint32_t rs1, uint32_t rd);
void insi_i_lhu(uint32_t imm, uint32_t rs1, uint32_t rd);

// (imm, rs1, rd)
void insi_i_jalr(uint32_t imm, uint32_t rs1, uint32_t rd);

// (imm, rs2, rs1)
void insi_s_sb(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_s_sh(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_s_sw(uint32_t imm, uint32_t rs2, uint32_t rs1);

// (imm, rs2, rs1)
void insi_b_beq(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_b_bne(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_b_blt(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_b_bge(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_b_bltu(uint32_t imm, uint32_t rs2, uint32_t rs1);
void insi_b_bgeu(uint32_t imm, uint32_t rs2, uint32_t rs1);

// (imm, rd)
void insi_u_lui(uint32_t imm, uint32_t rd);
void insi_u_auipc(uint32_t imm, uint32_t rd);

// (imm, rd)
void insi_j_jal(uint32_t imm, uint32_t rd);

// (void)
void insi_i_ecall(void);
void insi_i_ebreak(void);

#endif