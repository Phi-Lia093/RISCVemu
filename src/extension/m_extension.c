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
#include <extension/m_extension.h>
#include <limits.h>
#include <logger.h>
#include <mem.h>
#include <stdint.h>

#ifdef CONFIG_ENABLE_M_EXTENSION
void
insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    reg_write(rd, (uint32_t)(v1 * v2));
}

void
insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);
    int64_t result = (int64_t)v1 * (int64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    int64_t result = (int64_t)v1 * (uint64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);
    uint64_t result = (uint64_t)v1 * (uint64_t)v2;
    reg_write(rd, (uint32_t)(result >> 32));
}

void
insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, 0xFFFFFFFF);
    }
    else if (unlikely(v1 == INT32_MIN && v2 == -1))
    {
        reg_write(rd, (uint32_t)INT32_MIN);
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 / v2));
    }
}

void
insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, 0xFFFFFFFF);
    }
    else
    {
        reg_write(rd, v1 / v2);
    }
}

void
insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    int32_t v1 = (int32_t)reg_read(rs1);
    int32_t v2 = (int32_t)reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, v1);
    }
    else if (unlikely(v1 == INT32_MIN && v2 == -1))
    {
        reg_write(rd, 0);
    }
    else
    {
        reg_write(rd, (uint32_t)(v1 % v2));
    }
}

void
insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd)
{
    uint32_t v1 = reg_read(rs1);
    uint32_t v2 = reg_read(rs2);

    if (unlikely(v2 == 0))
    {
        reg_write(rd, v1);
    }
    else
    {
        reg_write(rd, v1 % v2);
    }
}
#endif
