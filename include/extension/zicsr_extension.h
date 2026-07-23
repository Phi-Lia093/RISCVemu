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

#ifndef ZICSR_EXTENSION_H
#define ZICSR_EXTENSION_H

#include <stdint.h>

#define RW 1
#define RO 0

struct csr_operation
{
    uint32_t valid;
    uint32_t privilege;
    uint32_t rw;
    uint32_t value;
    uint32_t (*read_callback)(void);
    void (*write_callback)(uint32_t val);
};

extern struct csr_operation csr_table[256];

void init_csr_table(void);

void ins_sret(void);
void ins_mret(void);
void ins_mnret(void);
void ins_wfi(void);
void ins_sctrclr(void);
void ins_sfence_vma(void);

void ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr);

void ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr);

#endif
