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

#ifndef EMU_H
#define EMU_H

#include <config.h>
#include <hashmap_u32.h>
#include <stdint.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// mmu flags
// bit 0: reserve (in "A" extension)

struct machine_state
{
    uint32_t gpr[32];
    uint32_t pc;
    uint8_t *main_memory;
    int terminated;
#ifdef CONFIG_ENABLE_A_EXTENSION
    struct hashmap mmu_flags;
#endif
#ifdef CONFIG_ENABLE_DEBUGGER
    int single_step;
    uint32_t breakpoint;
    uint32_t breakpoint_enabled;
#endif
};

extern struct machine_state g_state;

#endif
