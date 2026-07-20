#ifndef M_EXTENSION_H
#define M_EXTENSION_H

#include <stdint.h>

void insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd);

#endif
