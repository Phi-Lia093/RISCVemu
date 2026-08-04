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
#include <time.h>

uint64_t cycle = 0;
uint64_t instret = 0;

static inline uint64_t
get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
}

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
    return (uint32_t)get_time();
}

uint32_t
get_zicntr_time_h()
{
    return (uint32_t)(get_time() >> 32);
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

#endif // CONFIG_ENABLE_ZICNTR_EXTENSION
