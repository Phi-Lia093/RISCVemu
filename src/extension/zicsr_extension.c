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

#include <config.h>
#include <emu.h>
#include <exec.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#include <logger.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

struct csr_operation csr_table[4096] = { 0 };

void
init_csr_table(void)
{
    for (int i = 0; i < 4096; i++)
    {
        csr_table[i].valid = 0;
        csr_table[i].privilege = PRV_MACHINE;
        csr_table[i].rw = RW;
        csr_table[i].value = 0;
        csr_table[i].read_callback = NULL;
        csr_table[i].write_callback = NULL;
    }

    // Machine Information Registers
    csr_table[CSR_MVENDORID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, NULL, NULL };
    csr_table[CSR_MARCHID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, NULL, NULL };
    csr_table[CSR_MIMPID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, NULL, NULL };
    csr_table[CSR_MHARTID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, NULL, NULL };

    // Machine Trap Setup
    csr_table[CSR_MSTATUS]
        = (struct csr_operation){ 1, PRV_MACHINE,  RW,
                                  0, mstatus_read, mstatus_write };
    csr_table[CSR_MISA]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0x40001100, NULL, NULL };
    csr_table[CSR_MEDELEG]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MIDELEG]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MIE]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MTVEC]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MCOUNTEREN]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0xFFFFFFFF, NULL, NULL };

    // Machine Trap Handling
    csr_table[CSR_MSCRATCH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MEPC]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MCAUSE]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MTVAL]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MIP]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MNSTATUS]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };

    // Physical Memory Protection
    csr_table[CSR_PMPCFG0]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_PMPADDR0]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    for (int i = 1; i < 16; i++)
    {
        csr_table[CSR_PMPADDR0 + i]
            = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    }

    // Supervisor Trap Setup
    csr_table[CSR_SSTATUS]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW,
                                  0, sstatus_read,   sstatus_write };
    csr_table[CSR_SIE]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_STVEC]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SSCRATCH]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SEPC]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SCAUSE]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_STVAL]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SIP]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SATP]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
}

void
ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    uint32_t new_val = reg_read(rs1);
    csr_write(csr, new_val);
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    if (rs1 != 0)
    {
        uint32_t rs1_val = reg_read(rs1);
        uint32_t new_val = old_val | rs1_val;
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    if (rs1 != 0)
    {
        uint32_t rs1_val = reg_read(rs1);
        uint32_t new_val = old_val & ~rs1_val;
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    uint32_t new_val = uimm & 0x1F;
    csr_write(csr, new_val);
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    if (uimm != 0)
    {
        uint32_t new_val = old_val | (uimm & 0x1F);
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr)
{
    check_csr_access(csr);
    uint32_t old_val = csr_read(csr);
    if (uimm != 0)
    {
        uint32_t new_val = old_val & ~(uimm & 0x1F);
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

#endif
