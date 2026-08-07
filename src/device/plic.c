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

// Register map (all 32-bit, relative to PLIC_BASE).  Two hart contexts are
// modelled: context 0 is the machine-mode context used by OpenSBI, context 1
// is the supervisor-mode context used by Linux (the DT lists
// interrupts-extended = <&cpu_intc 11> <&cpu_intc 9>, so the S entry maps to
// context 1).  Registers:
//   priority[i]         RW : 0x000000 + 4*i          (i = 1..PLIC_NDEV)
//   pending             RO : 0x001000
//   enable[ctx]         RW : 0x002000 + 0x80*ctx
//   threshold[ctx]      RW : 0x200000 + 0x1000*ctx
//   claim/complete[ctx] RW : 0x200004 + 0x1000*ctx
#define PLIC_PRIORITY_END (0x4u * (PLIC_NDEV + 1u))
#define PLIC_PENDING_BASE 0x001000u
#define PLIC_ENABLE_BASE 0x002000u
#define PLIC_ENABLE_STRIDE 0x80u
#define PLIC_CONTEXT_BASE 0x200000u
#define PLIC_CONTEXT_STRIDE 0x1000u
#define PLIC_CLAIM_OFF 0x4u

#define PLIC_NCTX 2u /* context 0 = M-mode, context 1 = S-mode */

static uint32_t priority[PLIC_NDEV + 1];
static uint32_t pending; /* live level-sensitive device interrupt lines */
static uint32_t enable[PLIC_NCTX];
static uint32_t threshold[PLIC_NCTX];
static bool claimed[PLIC_NCTX][PLIC_NDEV + 1];

void
plic_init(void)
{
    for (unsigned i = 0; i <= PLIC_NDEV; i++)
    {
        priority[i] = 0;
        for (unsigned c = 0; c < PLIC_NCTX; c++) claimed[c][i] = false;
    }
    pending = 0;
    for (unsigned c = 0; c < PLIC_NCTX; c++)
    {
        enable[c] = 0;
        threshold[c] = 0;
    }
}

static unsigned
plic_best_source(unsigned ctx)
{
    // Highest enabled+priority pending source for one context (the PLIC
    // forwards only the top one).  A source currently claimed by this context
    // is masked out until it is completed.
    unsigned best = 0;
    unsigned best_pri = 0;
    for (unsigned id = 1; id <= PLIC_NDEV; id++)
    {
        if (!(pending & (1u << id))) continue;
        if (claimed[ctx][id]) continue;
        if (!(enable[ctx] & (1u << id))) continue;
        if (priority[id] <= threshold[ctx]) continue;
        if (priority[id] >= best_pri)
        {
            best_pri = priority[id];
            best = id;
        }
    }
    return best;
}

uint32_t
plic_read32(uint32_t off)
{
    if (off >= 0x4u && off < PLIC_PRIORITY_END)
    {
        // Interrupt source i has its priority register at offset 4*i (source 1
        // at 0x4, ..., source 10 at 0x28), per the PLIC spec and what Linux's
        // sifive-plic driver writes (PRIORITY_BASE + hwirq * 4).
        unsigned id = off / 4u;
        if (id >= 1u && id <= PLIC_NDEV) return priority[id];
        return 0;
    }
    if (off >= PLIC_PENDING_BASE && off < PLIC_PENDING_BASE + 4) return pending;
    if (off >= PLIC_ENABLE_BASE
        && off < PLIC_ENABLE_BASE + PLIC_ENABLE_STRIDE * PLIC_NCTX)
    {
        unsigned ctx = (off - PLIC_ENABLE_BASE) / PLIC_ENABLE_STRIDE;
        unsigned word = ((off - PLIC_ENABLE_BASE) % PLIC_ENABLE_STRIDE) / 4u;
        if (word == 0) return enable[ctx];
        return 0;
    }
    if (off >= PLIC_CONTEXT_BASE
        && off < PLIC_CONTEXT_BASE + PLIC_CONTEXT_STRIDE * PLIC_NCTX)
    {
        unsigned ctx = (off - PLIC_CONTEXT_BASE) / PLIC_CONTEXT_STRIDE;
        unsigned sub = (off - PLIC_CONTEXT_BASE) % PLIC_CONTEXT_STRIDE;
        if (sub == 0) return threshold[ctx];
        if (sub == PLIC_CLAIM_OFF)
        {
            // Claim: return the highest-priority pending enabled source for
            // this context and mark it in-progress.  The pending bit itself is
            // the live device line (not cleared by claim), so a level that is
            // still asserted re-arms the other context / this one after
            // complete -- matching real hardware behaviour.
            unsigned id = plic_best_source(ctx);
            if (id >= 1u && id <= PLIC_NDEV)
            {
                claimed[ctx][id] = true;
                return id;
            }
            return 0;
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
        unsigned id = off / 4u;
        if (id >= 1u && id <= PLIC_NDEV)
            priority[id] = val & 0x7u; /* 3-bit priority */
        return;
    }
    if (off >= PLIC_ENABLE_BASE
        && off < PLIC_ENABLE_BASE + PLIC_ENABLE_STRIDE * PLIC_NCTX)
    {
        unsigned ctx = (off - PLIC_ENABLE_BASE) / PLIC_ENABLE_STRIDE;
        unsigned word = ((off - PLIC_ENABLE_BASE) % PLIC_ENABLE_STRIDE) / 4u;
        if (word == 0) enable[ctx] = val;
        return;
    }
    if (off >= PLIC_CONTEXT_BASE
        && off < PLIC_CONTEXT_BASE + PLIC_CONTEXT_STRIDE * PLIC_NCTX)
    {
        unsigned ctx = (off - PLIC_CONTEXT_BASE) / PLIC_CONTEXT_STRIDE;
        unsigned sub = (off - PLIC_CONTEXT_BASE) % PLIC_CONTEXT_STRIDE;
        if (sub == 0)
        {
            threshold[ctx] = val & 0x7u;
            return;
        }
        if (sub == PLIC_CLAIM_OFF)
        {
            // Complete: reinstate the claim-free state for the completed
            // source in this context.
            unsigned id = val & 0x1Fu;
            if (id >= 1u && id <= PLIC_NDEV) claimed[ctx][id] = false;
        }
        return;
    }
    // Priority id 0, pending (read-only) and anything else: nothing to store.
}

void
plic_set_irq(unsigned id, bool level)
{
    if (id < 1u || id > PLIC_NDEV) return;
    // pending is the live level-sensitive device line.  Claiming in a context
    // records the in-progress source (masking it from that context via
    // plic_best_source) but does not clear the line, matching real hardware.
    if (level)
        pending |= (1u << id);
    else
        pending &= ~(1u << id);
}

unsigned
plic_external_pending(void)
{
    // Context 0 (M-mode) -- drives MIP.MEIP.
    return plic_best_source(0);
}

unsigned
plic_external_pending_s(void)
{
    // Context 1 (S-mode) -- drives MIP.SEIP.
    return plic_best_source(1);
}

unsigned
plic_irq_count(void)
{
    return PLIC_NDEV;
}
