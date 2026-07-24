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

#ifndef ZICOND_EXTENSION_H
#define ZICOND_EXTENSION_H

#include <config.h>
#ifdef CONFIG_ENABLE_ZICOND_EXTENSION

#include <exec.h>
#include <stdint.h>

static inline void
ins_zicond_czero_eqz(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t val1 = reg_read(rs1);
    uint32_t val2 = reg_read(rs2);
    if (val2 == 0)
        reg_write(rd, 0);
    else
        reg_write(rd, val1);
}

static inline void
ins_zicond_czero_nez(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t val1 = reg_read(rs1);
    uint32_t val2 = reg_read(rs2);
    if (val2 != 0)
        reg_write(rd, 0);
    else
        reg_write(rd, val1);
}

#endif

#endif
