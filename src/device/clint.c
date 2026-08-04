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

#include <device/clint.h>

// The machine timer is modeled as two 64-bit registers.  They are accessed at
// byte granularity (the memory bus routes each byte of a wide access through
// clint_store / clint_read), so the 64-bit values are stored directly and
// read/written byte-wise.

static uint64_t clint_mtime = 0;
static uint64_t clint_mtimecmp = 0; // 0 => compare disabled (no interrupt)

void
clint_tick(void)
{
    clint_mtime++;
}

bool
clint_mtip_pending(void)
{
    // Compare disabled when mtimecmp == 0.
    return clint_mtimecmp != 0 && clint_mtime >= clint_mtimecmp;
}

void
clint_store(uint32_t addr, uint8_t val)
{
    // mtimecmp (RW) at 0x02004000, eight bytes.
    if (addr >= CLINT_MTIMECMP_HART0 && addr < CLINT_MTIMECMP_HART0 + 8)
    {
        uint8_t byte = (uint8_t)(addr - CLINT_MTIMECMP_HART0);
        clint_mtimecmp &= ~((uint64_t)0xFFu << (byte * 8));
        clint_mtimecmp |= (uint64_t)val << (byte * 8);
        return;
    }
    // mtime (RO) is written; ignore (firmware never writes mtime).
}

int32_t
clint_read(uint32_t addr)
{
    // mtimecmp at 0x02004000.
    if (addr >= CLINT_MTIMECMP_HART0 && addr < CLINT_MTIMECMP_HART0 + 8)
    {
        uint8_t byte = (uint8_t)(addr - CLINT_MTIMECMP_HART0);
        return (int32_t)((clint_mtimecmp >> (byte * 8)) & 0xFFu);
    }
    // mtime at 0x0200BFF8.
    if (addr >= CLINT_MTIME_ADDR && addr < CLINT_MTIME_ADDR + 8)
    {
        uint8_t byte = (uint8_t)(addr - CLINT_MTIME_ADDR);
        return (int32_t)((clint_mtime >> (byte * 8)) & 0xFFu);
    }
    // Other CLINT registers (msip) return 0.
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        return 0;
    }
    return -1;
}
