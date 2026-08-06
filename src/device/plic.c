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

#include <device/plic.h>

// Register map (all 32-bit, relative to PLIC_BASE).  One hart context (0), so
// the enable window is at 0x02000 and the threshold/claim/complete at
// 0x200000 / 0x200004.
#define PLIC_PRIORITY_BASE 0x000000u + 0x4u
#define PLIC_PRIORITY_END 0x000000u + 0x4u * PLIC_NDEV
#define PLIC_PENDING_BASE 0x001000u
#define PLIC_ENABLE_BASE 0x002000u
#define PLIC_THRESHOLD 0x200000u
#define PLIC_CLAIM_COMPLETE 0x200004u

static uint32_t priority[PLIC_NDEV + 1];
static uint32_t pending; /* pending[id] bitmask (id 1..PLIC_NDEV) */
static bool claimed[PLIC_NDEV + 1];
static uint32_t enable; /* enable[id] bitmask for the single context */
static uint32_t threshold;

void
plic_init(void)
{
    for (unsigned i = 0; i <= PLIC_NDEV; i++)
    {
        priority[i] = 0;
        claimed[i] = false;
    }
    pending = 0;
    enable = 0;
    threshold = 0;
}

uint32_t
plic_read32(uint32_t off)
{
    if (off >= 0x4u && off < PLIC_PRIORITY_END)
    {
        unsigned id = (off - 0x4u) / 4u;
        if (id >= 1u && id <= PLIC_NDEV) return priority[id];
        return 0;
    }
    if (off >= PLIC_PENDING_BASE && off < PLIC_PENDING_BASE + 4) return pending;
    if (off >= PLIC_ENABLE_BASE && off < PLIC_ENABLE_BASE + 4) return enable;
    if (off == PLIC_THRESHOLD) return threshold;
    if (off == PLIC_CLAIM_COMPLETE)
    {
        // Claim: return the highest-priority pending enabled source and mark
        // it claimed, which deasserts MEIP (id 0 means none, per the spec).
        unsigned id = plic_external_pending();
        if (id >= 1u && id <= PLIC_NDEV)
        {
            pending &= ~(1u << id);
            claimed[id] = true;
            return id;
        }
        return 0;
    }
    return 0;
}

void
plic_write32(uint32_t off, uint32_t val)
{
    if (off >= 0x4u && off < PLIC_PRIORITY_END)
    {
        unsigned id = (off - 0x4u) / 4u;
        if (id >= 1u && id <= PLIC_NDEV)
            priority[id] = val & 0x7u; /* 3-bit priority */
        return;
    }
    if (off >= PLIC_ENABLE_BASE && off < PLIC_ENABLE_BASE + 4)
    {
        enable = val;
        return;
    }
    if (off == PLIC_THRESHOLD)
    {
        threshold = val & 0x7u;
        return;
    }
    if (off == PLIC_CLAIM_COMPLETE)
    {
        // Complete: reinstate the claim-free state for the completed source.
        unsigned id = val & 0x1Fu;
        if (id >= 1u && id <= PLIC_NDEV) claimed[id] = false;
        return;
    }
    // Priority id 0, pending (read-only) and anything else: nothing to store.
}

void
plic_set_irq(unsigned id, bool level)
{
    if (id < 1u || id > PLIC_NDEV) return;
    if (level)
    {
        // Do not re-raise an already-claimed source; the guest must complete
        // it (via a write to claim/complete) before it can be re-asserted.
        if (!claimed[id]) pending |= (1u << id);
    }
    else
    {
        pending &= ~(1u << id);
    }
}

unsigned
plic_external_pending(void)
{
    // Highest enabled+priority pending source (PLIC forwards the top one).
    unsigned best = 0;
    unsigned best_pri = 0;
    for (unsigned id = 1; id <= PLIC_NDEV; id++)
    {
        if (!(pending & (1u << id))) continue;
        if (!(enable & (1u << id))) continue;
        if (priority[id] <= threshold) continue;
        if (priority[id] >= best_pri)
        {
            best_pri = priority[id];
            best = id;
        }
    }
    return best;
}

unsigned
plic_irq_count(void)
{
    return PLIC_NDEV;
}
