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

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <stdint.h>

uint64_t fpr[sizeof(__float128) * 32 / sizeof(uint64_t)]; // max 32*QWORD

uint32_t fcsr;

uint32_t
get_fcsr()
{
    return fcsr;
}

uint32_t
get_fflags()
{
    return fcsr & 0b11111;
}

uint32_t
get_frm()
{
    return (fcsr >> 5) & 0b111;
}

void
set_fflags(uint32_t flags)
{
    fcsr = (fcsr & ~0b11111) | (flags & 0b11111);
}

void
set_frm(uint32_t frm)
{
    if (frm >= 0b100) return;
    fcsr = (fcsr & ~0b11100000) | (frm << 5);
}

void
set_fcsr(uint32_t csr)
{
    uint32_t new_frm = (csr >> 5) & 0b111;
    uint32_t new_fflags = csr & 0b11111;

    if (new_frm < 0b100)
    {
        fcsr = (new_fflags & 0b11111) | (new_frm << 5);
    }
    else
    {
        fcsr = (new_fflags & 0b11111) | (fcsr & 0b11100000);
    }
}

void
insf_flw(uint32_t imm, uint32_t rs1, uint32_t rd)
{
}

void
insf_fsw(uint32_t imm, uint32_t rs1, uint32_t rs2)
{
}

#endif
