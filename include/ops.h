#ifndef OPS_H
#define OPS_H

#include <stdint.h>

#define INSI_R_ADD 0
#define INSI_R_SUB 1
#define INSI_R_XOR 2
#define INSI_R_OR 3
#define INSI_R_AND 4
#define INSI_R_SLL 5
#define INSI_R_SRL 6
#define INSI_R_SRA 7
#define INSI_R_SLT 8
#define INSI_R_SLTU 9

#define INSM_R_MUL 10
#define INSM_R_MULH 11
#define INSM_R_MULSU 12
#define INSM_R_MULU 13
#define INSM_R_DIV 14
#define INSM_R_DIVU 15
#define INSM_R_REM 16
#define INSM_R_REMU 17

#define INSI_I_ADDI 18
#define INSI_I_XORI 19
#define INSI_I_ORI 20
#define INSI_I_ANDI 21
#define INSI_I_SLLI 22
#define INSI_I_SRLI 23
#define INSI_I_SRAI 24
#define INSI_I_SLTI 25
#define INSI_I_SLTIU 26

#define INSI_I_LB 27
#define INSI_I_LH 28
#define INSI_I_LW 29
#define INSI_I_LBU 30
#define INSI_I_LHU 31

#define INSI_I_JALR 32

#define INSI_I_ECALL 33
#define INSI_I_EBREAK 34

#define INSI_S_SB 35
#define INSI_S_SH 36
#define INSI_S_SW 37

#define INSI_B_BEQ 38
#define INSI_B_BNE 39
#define INSI_B_BLT 40
#define INSI_B_BGE 41
#define INSI_B_BLTU 42
#define INSI_B_BGEU 43

#define INSI_U_LUI 44
#define INSI_U_AUIPC 45

#define INSI_J_JAL 46

void insi_r_add(uint32_t, uint32_t, uint32_t);
void insi_r_sub(uint32_t, uint32_t, uint32_t);
void insi_r_xor(uint32_t, uint32_t, uint32_t);
void insi_r_or(uint32_t, uint32_t, uint32_t);
void insi_r_and(uint32_t, uint32_t, uint32_t);
void insi_r_sll(uint32_t, uint32_t, uint32_t);
void insi_r_srl(uint32_t, uint32_t, uint32_t);
void insi_r_sra(uint32_t, uint32_t, uint32_t);
void insi_r_slt(uint32_t, uint32_t, uint32_t);
void insi_r_sltu(uint32_t, uint32_t, uint32_t);

void insm_r_mul(uint32_t, uint32_t, uint32_t);
void insm_r_mulh(uint32_t, uint32_t, uint32_t);
void insm_r_mulsu(uint32_t, uint32_t, uint32_t);
void insm_r_mulu(uint32_t, uint32_t, uint32_t);
void insm_r_div(uint32_t, uint32_t, uint32_t);
void insm_r_divu(uint32_t, uint32_t, uint32_t);
void insm_r_rem(uint32_t, uint32_t, uint32_t);
void insm_r_remu(uint32_t, uint32_t, uint32_t);

void insi_i_addi(uint32_t, uint32_t, uint32_t);
void insi_i_xori(uint32_t, uint32_t, uint32_t);
void insi_i_ori(uint32_t, uint32_t, uint32_t);
void insi_i_andi(uint32_t, uint32_t, uint32_t);
void insi_i_slli(uint32_t, uint32_t, uint32_t);
void insi_i_srli(uint32_t, uint32_t, uint32_t);
void insi_i_srai(uint32_t, uint32_t, uint32_t);
void insi_i_slti(uint32_t, uint32_t, uint32_t);
void insi_i_sltiu(uint32_t, uint32_t, uint32_t);

void insi_i_lb(uint32_t, uint32_t, uint32_t);
void insi_i_lh(uint32_t, uint32_t, uint32_t);
void insi_i_lw(uint32_t, uint32_t, uint32_t);
void insi_i_lbu(uint32_t, uint32_t, uint32_t);
void insi_i_lhu(uint32_t, uint32_t, uint32_t);

void insi_i_jalr(uint32_t, uint32_t, uint32_t);
void insi_i_ecall(uint32_t, uint32_t, uint32_t);
void insi_i_ebreak(uint32_t, uint32_t, uint32_t);

void insi_s_sb(uint32_t, uint32_t, uint32_t);
void insi_s_sh(uint32_t, uint32_t, uint32_t);
void insi_s_sw(uint32_t, uint32_t, uint32_t);

void insi_b_beq(uint32_t, uint32_t, uint32_t);
void insi_b_bne(uint32_t, uint32_t, uint32_t);
void insi_b_blt(uint32_t, uint32_t, uint32_t);
void insi_b_bge(uint32_t, uint32_t, uint32_t);
void insi_b_bltu(uint32_t, uint32_t, uint32_t);
void insi_b_bgeu(uint32_t, uint32_t, uint32_t);

void insi_u_lui(uint32_t, uint32_t, uint32_t);
void insi_u_auipc(uint32_t, uint32_t, uint32_t);

void insi_j_jal(uint32_t, uint32_t, uint32_t);

void (*instructions[])(uint32_t, uint32_t, uint32_t) = {
    &insi_r_add,   &insi_r_sub,   &insi_r_xor,   &insi_r_or,    &insi_r_and,
    &insi_r_sll,   &insi_r_srl,   &insi_r_sra,   &insi_r_slt,   &insi_r_sltu,
    &insm_r_mul,   &insm_r_mulh,  &insm_r_mulsu, &insm_r_mulu,  &insm_r_div,
    &insm_r_divu,  &insm_r_rem,   &insm_r_remu,  &insi_i_addi,  &insi_i_xori,
    &insi_i_ori,   &insi_i_andi,  &insi_i_slli,  &insi_i_srli,  &insi_i_srai,
    &insi_i_slti,  &insi_i_sltiu, &insi_i_lb,    &insi_i_lh,    &insi_i_lw,
    &insi_i_lbu,   &insi_i_lhu,   &insi_i_jalr,  &insi_i_ecall, &insi_i_ebreak,
    &insi_s_sb,    &insi_s_sh,    &insi_s_sw,    &insi_b_beq,   &insi_b_bne,
    &insi_b_blt,   &insi_b_bge,   &insi_b_bltu,  &insi_b_bgeu,  &insi_u_lui,
    &insi_u_auipc, &insi_j_jal
};

//					R   rs2        rs1       rd
//					I   imm        rs1       rd
//					S   imm        rs2       rs1
//					B   imm        rs2       rs1
//					U   imm        N/A       rd
//					J   imm        N/A       rd
#endif