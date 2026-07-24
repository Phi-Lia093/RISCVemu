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

#include <emu.h>
#include <logger.h>
#include <stdint.h>

#define CSR_MSTATUS 0x300
#define CSR_MISA 0x301
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305
#define CSR_MCOUNTEREN 0x306

#define CSR_MSCRATCH 0x340
#define CSR_MEPC 0x341
#define CSR_MCAUSE 0x342
#define CSR_MTVAL 0x343
#define CSR_MIP 0x344
#define CSR_MNSTATUS 0x744

#define CSR_PMPCFG0 0x3A0
#define CSR_PMPADDR0 0x3B0

#define CSR_SSTATUS 0x100
#define CSR_SIE 0x104
#define CSR_STVEC 0x105
#define CSR_SSCRATCH 0x140
#define CSR_SEPC 0x141
#define CSR_SCAUSE 0x142
#define CSR_STVAL 0x143
#define CSR_SIP 0x144
#define CSR_SATP 0x180

#ifndef CSR_MVENDORID
#define CSR_MVENDORID 0xF11
#define CSR_MARCHID 0xF12
#define CSR_MIMPID 0xF13
#define CSR_MHARTID 0xF14
#define CSR_MCONFIGPTR 0xF15
#endif

#define CSR_PMPCFG0 0x3A0
#define CSR_PMPADDR0 0x3B0
#define CSR_PMPADDR1 0x3B1
#define CSR_PMPADDR2 0x3B2
#define CSR_PMPADDR3 0x3B3
#define CSR_PMPADDR4 0x3B4
#define CSR_PMPADDR5 0x3B5
#define CSR_PMPADDR6 0x3B6
#define CSR_PMPADDR7 0x3B7
#define CSR_PMPADDR8 0x3B8
#define CSR_PMPADDR9 0x3B9
#define CSR_PMPADDR10 0x3BA
#define CSR_PMPADDR11 0x3BB
#define CSR_PMPADDR12 0x3BC
#define CSR_PMPADDR13 0x3BD
#define CSR_PMPADDR14 0x3BE
#define CSR_PMPADDR15 0x3BF

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

extern struct csr_operation csr_table[4096];

static inline uint32_t
csr_read(uint32_t csr)
{
    return csr_table[csr].read_callback ? csr_table[csr].read_callback()
                                        : csr_table[csr].value;
}

static inline void
csr_write(uint32_t csr, uint32_t val)
{
    if (csr_table[csr].rw == RO)
    {
        fatal("Write to read-only CSR");
    }
    if (csr_table[csr].write_callback)
    {
        csr_table[csr].write_callback(val);
    }
    else
    {
        csr_table[csr].value = val;
    }
}

static inline void
check_csr_access(uint32_t csr)
{
    if (!csr_table[csr].valid)
    {
        fatal("Invalid CSR access: 0x%03X", csr);
    }
    if (g_state.privilege < csr_table[csr].privilege)
    {
        fatal(
            "Insufficient privilege for CSR access: 0x%03X (need %d, have %d)",
            csr, csr_table[csr].privilege, g_state.privilege);
    }
}

void init_csr_table(void);

void ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr);

void ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr);
void ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr);

#endif
