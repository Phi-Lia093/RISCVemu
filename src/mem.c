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

#include <hashmap_u32.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <config.h>
#include <logger.h>
#include <mem.h>

void
init_mem(void)
{
    g_state.main_memory = (uint8_t *)calloc(MEM_SIZE, sizeof(uint8_t));
#ifdef CONFIG_ENABLE_A_EXTENSION
    hashmap_init(&g_state.mmu_flags);
#endif
    if (!g_state.main_memory)
    {
        fatal("Failed to allocate memory");
    }
}
