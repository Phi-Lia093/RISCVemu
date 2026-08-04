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

#ifndef F_EXTENSION_H
#define F_EXTENSION_H

#include <config.h>

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <softfloat.h>
#include <stdint.h>

extern uint64_t fpr[sizeof(__float128) * 32 / sizeof(uint64_t)]; // max 32*QWORD

extern uint32_t fcsr;

uint32_t get_fflags(void);
uint32_t get_frm(void);
uint32_t get_fcsr(void);

void set_fflags(uint32_t flags);
void set_frm(uint32_t frm);
void set_fcsr(uint32_t csr);

#include <emu.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
// mstatus.FS == 0 means the FPU is "off": any floating-point instruction
// (including FP loads/stores) raises an illegal-instruction exception.
static inline int
fpu_fs_off(void)
{
    return (csr_read(CSR_MSTATUS) & MSTATUS_FS_MASK) == 0;
}

#define CSR_FFLAGS 0x001
#define CSR_FRM 0x002
#define CSR_FCSR 0x003

#define RNE 0b000 // round to nearest
#define RTZ 0b001 // round towards zero
#define RDN 0b010 // round down
#define RUP 0b011 // round up
#define RMM 0b100 // round to nearest even
#define DYN 0b111 // dynamic rounding mode

#define NX 0b1     // non-exact
#define UF 0b10    // underflow
#define OF 0b100   // overflow
#define DZ 0b1000  // divide by zero
#define NV 0b10000 // invalid operation

#define S 0b00 // single precision
#define D 0b01 // double precision
#define H 0b10 // half precision
#define Q 0b11 // quad precision

static inline uint32_t
get_rm(uint32_t rm)
{
    return (rm == DYN) ? (fcsr >> 5) & 0b111 : rm;
}

/* FPR accessors - exported for debugger and other modules */
uint32_t fpr_read_s(uint32_t r);
uint64_t fpr_read_d(uint32_t r);
uint16_t fpr_read_h(uint32_t r);
void fpr_read_q(uint32_t r, float128_t *out);
void fpr_write_s(uint32_t r, uint32_t v);
void fpr_write_d(uint32_t r, uint64_t v);
void fpr_write_h(uint32_t r, uint16_t v);
void fpr_write_q(uint32_t r, const float128_t *in);

/* FP load / store */
void insf_flw(uint32_t imm, uint32_t rs1, uint32_t rd);
void insf_fsw(uint32_t imm, uint32_t rs1, uint32_t rs2);
void insf_fld(uint32_t imm, uint32_t rs1, uint32_t rd);
void insf_fsd(uint32_t imm, uint32_t rs1, uint32_t rs2);
void insf_flh(uint32_t imm, uint32_t rs1, uint32_t rd);
void insf_fsh(uint32_t imm, uint32_t rs1, uint32_t rs2);
void insf_flq(uint32_t imm, uint32_t rs1, uint32_t rd);
void insf_fsq(uint32_t imm, uint32_t rs1, uint32_t rs2);

/* Fused multiply-add (opcode 0x43 family).
 * subop: 0=fmadd, 1=fmsub, 2=fnmsub, 3=fnmadd.
 * rs3 is the addend register; rs1/rs2 are the multiplicands. */
void insf_r_fma(uint32_t ins, uint32_t subop);

/* FP-OP (opcode 0x53). ins is the full instruction word. */
void insf_r_fpop(uint32_t ins);

#endif // CONFIG_ENABLE_F_EXTENSION

#endif // F_EXTENSION_H
