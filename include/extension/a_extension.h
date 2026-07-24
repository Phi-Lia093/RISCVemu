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

#ifndef A_EXTENSION_H
#define A_EXTENSION_H

#include <config.h>

#ifdef CONFIG_ENABLE_A_EXTENSION

#include <emu.h>
#include <exec.h>
#include <hashmap_u32.h>
#include <logger.h>
#include <mem.h>
#include <stdint.h>

static inline uint32_t
get_reservation_key(uint32_t addr)
{
    return addr & ~0x3;
}

static inline uint8_t
get_reserve_status(uint32_t addr)
{
    uint32_t key = get_reservation_key(addr);
    uint32_t value;
    if (!hashmap_get(&g_state.mmu_flags, key, &value))
    {
        return 0;
    }
    return (value & 1) ? 1 : 0;
}

static inline void
set_reserve_status(uint32_t addr)
{
    uint32_t key = get_reservation_key(addr);
    uint32_t value;
    if (!hashmap_get(&g_state.mmu_flags, key, &value))
    {
        value = 0;
    }
    value |= 1;
    hashmap_put(&g_state.mmu_flags, key, value);
}

static inline void
clear_reserve_status(uint32_t addr)
{
    uint32_t key = get_reservation_key(addr);
    uint32_t value;
    if (!hashmap_get(&g_state.mmu_flags, key, &value))
    {
        return;
    }
    value &= ~1;
    if (value == 0)
    {
        hashmap_remove(&g_state.mmu_flags, key);
    }
    else
    {
        hashmap_put(&g_state.mmu_flags, key, value);
    }
}

static inline void
insa_r_lr_w(uint32_t rs1, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);

    if (unlikely(addr & 0x3))
    {
        fatal("LR.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    reg_write(rd, mem_read32_unsigned(addr));
    set_reserve_status(addr);
}

static inline void
insa_r_sc_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("SC.W misaligned address: 0x%x", addr);
        reg_write(rd, 1);
        return;
    }

    if (get_reserve_status(addr))
    {
        mem_write32(addr, value);
        reg_write(rd, 0);
        clear_reserve_status(addr);
    }
    else
    {
        reg_write(rd, 1);
    }
}

static inline void
insa_r_amoswap_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOSWAP.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    mem_write32(addr, value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amoadd_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOADD.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    mem_write32(addr, old_value + value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amoand_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOAND.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    mem_write32(addr, old_value & value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amoor_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOOR.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    mem_write32(addr, old_value | value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amoxor_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOXOR.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    mem_write32(addr, old_value ^ value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amomax_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOMAX.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    int32_t signed_old = (int32_t)old_value;
    int32_t signed_val = (int32_t)value;
    uint32_t new_value = (signed_old > signed_val) ? old_value : value;
    mem_write32(addr, new_value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amomin_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOMIN.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    int32_t signed_old = (int32_t)old_value;
    int32_t signed_val = (int32_t)value;
    uint32_t new_value = (signed_old < signed_val) ? old_value : value;
    mem_write32(addr, new_value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amomaxu_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOMAXU.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    uint32_t new_value = (old_value > value) ? old_value : value;
    mem_write32(addr, new_value);
    reg_write(rd, old_value);
}

static inline void
insa_r_amominu_w(uint32_t rs1, uint32_t rs2, uint32_t rd)
{
    uint32_t addr = reg_read(rs1);
    uint32_t value = reg_read(rs2);

    if (unlikely(addr & 0x3))
    {
        fatal("AMOMINU.W misaligned address: 0x%x", addr);
        reg_write(rd, 0);
        return;
    }

    uint32_t old_value = mem_read32_unsigned(addr);
    uint32_t new_value = (old_value < value) ? old_value : value;
    mem_write32(addr, new_value);
    reg_write(rd, old_value);
}

#endif // CONFIG_ENABLE_A_EXTENSION

#endif // A_EXTENSION_H
