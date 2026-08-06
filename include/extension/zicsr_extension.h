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

#include <config.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

#include <emu.h>
#include <extension/system.h>
#include <logger.h>
#include <stdint.h>

#define CSR_MSTATUS 0x300
#define CSR_MISA 0x301
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305
#define CSR_MCOUNTEREN 0x306
// mstatush (0x310): upper 32 bits of mstatus on RV32 (SXL/UXL/UBE etc.).
#define CSR_MSTATUSH 0x310
// Machine-mode environment configuration (RISC-V 1.12 + Smstateen / envcfg).
#define CSR_MENVCFG 0x30A
#define CSR_MENVCFGH 0x31A

#define CSR_MSCRATCH 0x340
#define CSR_MEPC 0x341
#define CSR_MCAUSE 0x342
#define CSR_MTVAL 0x343
#define CSR_MIP 0x344
#define CSR_MNSTATUS 0x744

// Machine-mode counter/hang CSRs (writable in M-mode)
#define CSR_MCOUNTINHIBIT 0x320
#define CSR_MCYCLE 0xB00
#define CSR_MCYCLEH 0xB80
#define CSR_MINSTRET 0xB02
#define CSR_MINSTRETH 0xB82

// Sdtrig trigger CSRs (M-mode debug triggers)
#define CSR_TSELECT 0x7A0
#define CSR_TDATA1 0x7A1
#define CSR_TDATA2 0x7A2
#define CSR_TCONTROL 0x7A5

#define CSR_CYCLE_LO 0xC00
#define CSR_CYCLE_HI 0xC80
#define CSR_TIME_LO 0xC01
#define CSR_TIME_HI 0xC81
#define CSR_INSTRET_LO 0xC02
#define CSR_INSTRET_HI 0xC82

#define CSR_PMPCFG0 0x3A0
#define CSR_PMPCFG1 0x3A1
#define CSR_PMPCFG2 0x3A2
#define CSR_PMPCFG3 0x3A3
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

#define CSR_SSTATUS 0x100
#define CSR_SIE 0x104
#define CSR_STVEC 0x105
#define CSR_SSCRATCH 0x140
#define CSR_SEPC 0x141
#define CSR_SCAUSE 0x142
#define CSR_STVAL 0x143
#define CSR_SIP 0x144
#define CSR_SATP 0x180
// scounteren (0x106) / senvcfg (0x10A) and RV32 high half senvcfgh (0x11A).
#define CSR_SENVCFG 0x10A
#define CSR_SENVCFGH 0x11A

#define CSR_MVENDORID 0xF11
#define CSR_MARCHID 0xF12
#define CSR_MIMPID 0xF13
#define CSR_MHARTID 0xF14
#define CSR_MCONFIGPTR 0xF15

// Machine Security Configuration (MSECCFG) and scratch for NMI handler.
#define CSR_MSECCFG 0x747
#define CSR_MSECCFGH 0x757
#define CSR_MNSCRATCH 0x740

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
        // Per RISC-V spec, writes to read-only CSRs are ignored (no trap).
        return;
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
check_csr_access(uint32_t csr, uint32_t ins)
{
    if (!csr_table[csr].valid)
    {
        raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
        return;
    }
    if (g_state.privilege < csr_table[csr].privilege)
    {
        raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
        return;
    }
    // When mstatus.TVM is set, S-mode reads/writes of satp are illegal.
    if (csr == CSR_SATP && g_state.privilege == PRV_SUPERVISOR
        && (csr_table[CSR_MSTATUS].value & MSTATUS_TVM))
    {
        raise_exception(CAUSE_ILLEGAL_INSTRUCTION, ins);
        return;
    }
}

void init_csr_table(void);

void ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins);
void ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins);
void ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins);

void ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins);
void ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins);
void ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins);

#endif // CONFIG_ENABLE_ZICSR_EXTENSION
#endif // ZICSR_EXTENSION_H
