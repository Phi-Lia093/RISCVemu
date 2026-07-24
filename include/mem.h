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

#ifndef MEM_H
#define MEM_H

#include <stdbool.h>
#include <stdint.h>

#include <config.h>
#include <emu.h>

#define MEM_SIZE (4 * 1024 * 1024 * 1024L - 4096L)

#ifdef CONFIG_ENABLE_UART_DEVICE
#include <device/uart.h>
#endif

void init_mem(void);

static inline uint32_t
mem_read8_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return g_state.main_memory[addr];
}

static inline int32_t
mem_read8_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return (int32_t)(int8_t)g_state.main_memory[addr];
}

static inline void
mem_write8(uint32_t addr, uint8_t val)
{
    if (unlikely(addr >= MEM_SIZE)) return;
    g_state.main_memory[addr] = val;
}

static inline uint32_t
mem_read16_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    return *(uint16_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read16_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    return (int32_t)(int16_t)(*(uint16_t *)(g_state.main_memory + addr));
}

static inline void
mem_write16(uint32_t addr, uint16_t val)
{
    if (unlikely(addr >= MEM_SIZE - 1)) return;
    *(uint16_t *)(g_state.main_memory + addr) = val;
}

static inline uint32_t
mem_read32_unsigned(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    return *(uint32_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read32_signed(uint32_t addr)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    return *(int32_t *)(g_state.main_memory + addr);
}

static inline void
mem_write32(uint32_t addr, uint32_t val)
{
    if (unlikely(addr >= MEM_SIZE - 3)) return;
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (unlikely(addr == UART_OUT)) uart_putc(val);
#endif
    *(uint32_t *)(g_state.main_memory + addr) = val;
}

static inline bool
is_aligned(uint32_t addr, uint32_t size)
{
    return (addr & (size - 1)) == 0;
}

#ifdef CONFIG_SUPPORT_MISALIGN

static inline uint32_t
misaligned_load32(uint32_t addr)
{
    uint32_t val = 0;
    for (int i = 0; i < 4; i++)
    {
        val |= (uint32_t)mem_read8_unsigned(addr + i) << (i * 8);
    }
    return val;
}

static inline int32_t
misaligned_load32_signed(uint32_t addr)
{
    return (int32_t)misaligned_load32(addr);
}

static inline uint16_t
misaligned_load16(uint32_t addr)
{
    uint16_t val = 0;
    val |= (uint16_t)mem_read8_unsigned(addr);
    val |= (uint16_t)mem_read8_unsigned(addr + 1) << 8;
    return val;
}

static inline int16_t
misaligned_load16_signed(uint32_t addr)
{
    return (int16_t)misaligned_load16(addr);
}

static inline void
misaligned_store32(uint32_t addr, uint32_t val)
{
    for (int i = 0; i < 4; i++)
    {
        mem_write8(addr + i, (val >> (i * 8)) & 0xFF);
    }
}

static inline void
misaligned_store16(uint32_t addr, uint16_t val)
{
    mem_write8(addr, val & 0xFF);
    mem_write8(addr + 1, (val >> 8) & 0xFF);
}

#endif

#endif
