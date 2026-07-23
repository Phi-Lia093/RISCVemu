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
#include <extension/zicsr_extension.h>
#include <logger.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

struct csr_operation csr_table[256] = { 0 };

void
init_csr_table(void)
{
    csr_table[1] = (struct csr_operation){ .valid = 1,
                                           .privilege = PRV_USER,
                                           .rw = RW,
                                           .value = 0,
                                           .read_callback = NULL,
                                           .write_callback = NULL };
}

void
ins_sret(void)
{
}

void
ins_mret(void)
{
}

void
ins_mnret(void)
{
}

void
ins_wfi(void)
{
}

void
ins_sctrclr(void)
{
}

void
ins_sfence_vma(void)
{
}

void
check_csr_access(uint32_t csr)
{
    uint32_t index = csr & 0xFF;

    if (!csr_table[index].valid)
    {
        fatal("Invalid CSR access");
    }

    if (g_state.privilege > csr_table[index].privilege)
    {
        fatal("Insufficient privilege for CSR access");
    }
}

uint32_t
csr_read(uint32_t csr)
{
    uint32_t index = csr & 0xFF;
    check_csr_access(csr);
    if (csr_table[index].read_callback != NULL)
    {
        return csr_table[index].read_callback();
    }
    else
    {
        return csr_table[index].value;
    }
}

void
csr_write(uint32_t csr, uint32_t val)
{
    uint32_t index = csr & 0xFF;
    check_csr_access(csr);

    if (csr_table[index].rw == RO)
    {
        fatal("Write to read-only CSR");
    }

    if (csr_table[index].write_callback != NULL)
    {
        csr_table[index].write_callback(val);
    }
    else
    {
        csr_table[index].value = val;
    }
}

void
ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr)
{
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
