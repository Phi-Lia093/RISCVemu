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

#ifdef CONFIG_ENABLE_UART_DEVICE
#include <device/uart.h>
#endif
#ifdef CONFIG_ENABLE_PLIC_DEVICE
#include <device/plic.h>
#endif
#include <device/clint.h>

#define MEM_SIZE (4 * 1024 * 1024 * 1024L - 4096L)

void init_mem(void);

static inline uint32_t
mem_read8_unsigned(uint32_t addr)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        int32_t v = clint_read(addr);
        return v < 0 ? 0 : (uint32_t)v;
    }
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (addr >= UART_BASE && addr < UART_BASE + UART_NREGS)
    {
        return (uint32_t)uart_read(addr - UART_BASE);
    }
#endif
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    if (addr >= PLIC_BASE && addr < PLIC_BASE + 0x10000)
    {
        uint32_t w = plic_read32(addr - PLIC_BASE);
        return (w >> ((addr & 3u) * 8)) & 0xFFu;
    }
#endif
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return g_state.main_memory[addr];
}

static inline int32_t
mem_read8_signed(uint32_t addr)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
        return clint_read(addr);
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (addr >= UART_BASE && addr < UART_BASE + UART_NREGS)
        return (int32_t)(uint8_t)uart_read(addr - UART_BASE);
#endif
    if (unlikely(addr >= MEM_SIZE)) return 0;
    return (int32_t)(int8_t)g_state.main_memory[addr];
}

static inline void
mem_write8(uint32_t addr, uint8_t val)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        clint_store(addr, val);
        return;
    }
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (addr >= UART_BASE && addr < UART_BASE + UART_NREGS)
    {
        uart_write(addr - UART_BASE, val);
        return;
    }
#endif
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    if (addr >= PLIC_BASE && addr < PLIC_BASE + 0x10000)
    {
        uint32_t off = addr - PLIC_BASE;
        /* PLIC registers are 32-bit; the RISC-V PLIC spec says registers are
         * word addressed and that accesses are only defined at word
         * granularity.  Merge a byte write into the aligned word to be safe. */
        uint32_t cur = plic_read32(off & ~3u);
        cur &= ~(0xFFu << ((off & 3u) * 8));
        cur |= (uint32_t)val << ((off & 3u) * 8);
        plic_write32(off & ~3u, cur);
        return;
    }
#endif
    if (unlikely(addr >= MEM_SIZE)) return;
    g_state.main_memory[addr] = val;
}

static inline uint32_t
mem_read16_unsigned(uint32_t addr)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        uint32_t v = 0;
        for (int i = 0; i < 2; i++)
            v |= (uint32_t)(uint8_t)clint_read(addr + i) << (i * 8);
        return v;
    }
    if (unlikely(addr >= MEM_SIZE - 1)) return 0;
    return *(uint16_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read16_signed(uint32_t addr)
{
    return (int32_t)(int16_t)(uint16_t)mem_read16_unsigned(addr);
}

static inline void
mem_write16(uint32_t addr, uint16_t val)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        for (int i = 0; i < 2; i++)
            clint_store(addr + i, (uint8_t)(val >> (i * 8)));
        return;
    }
    if (unlikely(addr >= MEM_SIZE - 1)) return;
    *(uint16_t *)(g_state.main_memory + addr) = val;
}

static inline uint32_t
mem_read32_unsigned(uint32_t addr)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++)
            v |= (uint32_t)(uint8_t)clint_read(addr + i) << (i * 8);
        return v;
    }
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (addr >= UART_BASE && addr < UART_BASE + UART_NREGS)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4 && (addr + i) < UART_BASE + UART_NREGS; i++)
            v |= (uint32_t)uart_read(addr - UART_BASE + i) << (i * 8);
        return v;
    }
#endif
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    if (addr >= PLIC_BASE && addr < PLIC_BASE + PLIC_SIZE)
    {
        /* PLIC registers are 32-bit; the RISC-V PLIC spec requires that the
         * driver issue word accesses, but a 32-bit CPU may split into smaller
         * transactions, so gather bytes from the word read. */
        return plic_read32(addr - PLIC_BASE);
    }
#endif
    if (unlikely(addr >= MEM_SIZE - 3)) return 0;
    return *(uint32_t *)(g_state.main_memory + addr);
}

static inline int32_t
mem_read32_signed(uint32_t addr)
{
    return (int32_t)mem_read32_unsigned(addr);
}

static inline void
mem_write32(uint32_t addr, uint32_t val)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        for (int i = 0; i < 4; i++)
            clint_store(addr + i, (uint8_t)(val >> (i * 8)));
        return;
    }
#ifdef CONFIG_ENABLE_UART_DEVICE
    if (addr >= UART_BASE && addr < UART_BASE + UART_NREGS)
    {
        for (int i = 0; i < 4 && (addr + i) < UART_BASE + UART_NREGS; i++)
            uart_write(addr - UART_BASE + i, (uint8_t)(val >> (i * 8)));
        return;
    }
#endif
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    if (addr >= PLIC_BASE && addr < PLIC_BASE + PLIC_SIZE)
    {
        plic_write32(addr - PLIC_BASE, val);
        return;
    }
#endif
    if (unlikely(addr >= MEM_SIZE - 3)) return;
    *(uint32_t *)(g_state.main_memory + addr) = val;
}

static inline uint64_t
mem_read64_unsigned(uint32_t addr)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        uint64_t v = 0;
        for (int i = 0; i < 8; i++)
            v |= (uint64_t)(uint8_t)clint_read(addr + i) << (i * 8);
        return v;
    }
    if (unlikely(addr >= MEM_SIZE - 7)) return 0;
    return *(uint64_t *)(g_state.main_memory + addr);
}

static inline void
mem_write64(uint32_t addr, uint64_t val)
{
    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE)
    {
        for (int i = 0; i < 8; i++)
            clint_store(addr + i, (uint8_t)(val >> (i * 8)));
        return;
    }
    if (unlikely(addr >= MEM_SIZE - 7)) return;
    *(uint64_t *)(g_state.main_memory + addr) = val;
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
