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
#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION

#include <stdint.h>

#include <device/clint.h>

uint64_t cycle = 0;
uint64_t instret = 0;

uint32_t
get_zicntr_cycle_l()
{
    return (uint32_t)cycle;
}

uint32_t
get_zicntr_cycle_h()
{
    return (uint32_t)(cycle >> 32);
}

uint32_t
get_zicntr_time_l()
{
    // The architectural `time` CSR must report the same free-running counter
    // that drives the CLINT comparator (MIP.MTIP), otherwise firmware that
    // programs mtimecmp from rdtime would compute a deadline against a
    // different clock and the timer would never (or wildly late) fire.
    uint64_t mtime = clint_get_mtime();
    return (uint32_t)mtime;
}

uint32_t
get_zicntr_time_h()
{
    return (uint32_t)(clint_get_mtime() >> 32);
}

uint32_t
get_zicntr_instret_l()
{
    return (uint32_t)instret;
}

uint32_t
get_zicntr_instret_h()
{
    return (uint32_t)(instret >> 32);
}

// Per RISC-V, writing minstret/minstreth writes the low/high 32 bits of the
// architectural instret counter, and that instruction's own increment is
// suppressed. We expose a global flag that the main loop consults to skip the
// increment once, implementing the "overflow suppression" behavior exercised by
// rv32mi/instret_overflow.S.
uint32_t instret_suppress_next = 0;

void
set_zicntr_minstret_l(uint32_t val)
{
    instret = (instret & 0xFFFFFFFF00000000ULL) | (uint64_t)val;
    instret_suppress_next = 1;
}

void
set_zicntr_minstret_h(uint32_t val)
{
    instret = (instret & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32);
    instret_suppress_next = 1;
}

#endif // CONFIG_ENABLE_ZICNTR_EXTENSION
