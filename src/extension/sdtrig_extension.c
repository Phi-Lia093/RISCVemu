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

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

#include <stdint.h>

#include <emu.h>
#include <extension/sdtrig_extension.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#include <logger.h>

// Sdtrig trigger state.
static struct trigger_slot
{
    uint32_t tdata1;
    uint32_t tdata2;
} s_slots[SDTRIG_NUM_TRIGGERS];

static uint32_t s_tselect = 0;
static uint32_t s_tcontrol = 0;

// CSR callbacks
static uint32_t
get_tselect(void)
{
    return s_tselect;
}

static void
set_tselect(uint32_t val)
{
    if (val >= SDTRIG_NUM_TRIGGERS)
    {
        s_tselect = SDTRIG_NUM_TRIGGERS - 1;
        return;
    }
    s_tselect = val;
}

static uint32_t
get_tdata1(void)
{
    return s_slots[s_tselect].tdata1;
}

static void
set_tdata1(uint32_t val)
{
    s_slots[s_tselect].tdata1 = val & ~SDTRIG_DMODE;
}

static uint32_t
get_tdata2(void)
{
    return s_slots[s_tselect].tdata2;
}

static void
set_tdata2(uint32_t val)
{
    s_slots[s_tselect].tdata2 = val;
}

static uint32_t
get_tcontrol(void)
{
    return s_tcontrol;
}

static void
set_tcontrol(uint32_t val)
{
    s_tcontrol = val;
}

void
init_sdtrig_csr_table(void)
{
    for (int i = 0; i < SDTRIG_NUM_TRIGGERS; i++)
    {
        s_slots[i].tdata1 = 0;
        s_slots[i].tdata2 = 0;
    }
    s_tselect = 0;
    s_tcontrol = 0;

    csr_table[CSR_TSELECT]
        = (struct csr_operation){ 1, PRV_MACHINE, RW,
                                  0, get_tselect, set_tselect };
    csr_table[CSR_TDATA1] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_tdata1, set_tdata1
    };
    csr_table[CSR_TDATA2] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_tdata2, set_tdata2
    };
    csr_table[CSR_TCONTROL]
        = (struct csr_operation){ 1, PRV_MACHINE,  RW,
                                  0, get_tcontrol, set_tcontrol };
}

// ---------------------------------------------------------------------------
// Matching
// ---------------------------------------------------------------------------

static inline int
trigger_matches(int i, uint32_t addr, uint32_t access_type)
{
    uint32_t tdata1 = s_slots[i].tdata1;

    if (((tdata1 >> SDTRIG_TYPE_SHIFT) & 0xF) != SDTRIG_TYPE_MATCH)
    {
        return 0;
    }
    if (((tdata1 & SDTRIG_ACTION_MASK) >> SDTRIG_ACTION_SHIFT)
        != SDTRIG_ACTION_DEBUG_EXCEPTION)
    {
        return 0;
    }
    if (tdata1 & SDTRIG_CHAIN)
    {
        return 0;
    }

    if (access_type == SDTRIG_LOAD && !(tdata1 & SDTRIG_LOAD)) return 0;
    if (access_type == SDTRIG_STORE && !(tdata1 & SDTRIG_STORE)) return 0;
    if (access_type == SDTRIG_EXECUTE && !(tdata1 & SDTRIG_EXECUTE)) return 0;

    if (g_state.privilege == PRV_MACHINE)
    {
        if (!(tdata1 & SDTRIG_MODE_M)) return 0;
        if (!(s_tcontrol & SDTRIG_TCONTROL_MTE)) return 0;
    }
    else if (g_state.privilege == PRV_SUPERVISOR)
    {
        if (!(tdata1 & SDTRIG_MODE_S)) return 0;
    }
    else if (g_state.privilege == PRV_USER)
    {
        if (!(tdata1 & SDTRIG_MODE_U)) return 0;
    }
    else
    {
        return 0;
    }

    if ((tdata1 & SDTRIG_MATCH_MASK) != 0)
    {
        return 0;
    }

    return s_slots[i].tdata2 == addr;
}

static inline int
sdtrig_check(uint32_t addr, uint32_t access_type)
{
    for (int i = 0; i < SDTRIG_NUM_TRIGGERS; i++)
    {
        if (trigger_matches(i, addr, access_type))
        {
            raise_exception(CAUSE_BREAKPOINT, addr);
            debug("trigger %d fired: type=%s addr=0x%08X pc=0x%08X", i,
                  access_type == SDTRIG_EXECUTE
                      ? "execute"
                      : (access_type == SDTRIG_LOAD ? "load" : "store"),
                  addr, g_state.pc);
            return 1;
        }
    }
    return 0;
}

int
sdtrig_check_fetch_trigger(uint32_t pc)
{
    return sdtrig_check(pc, SDTRIG_EXECUTE);
}

int
sdtrig_check_load_trigger(uint32_t addr)
{
    return sdtrig_check(addr, SDTRIG_LOAD);
}

int
sdtrig_check_store_trigger(uint32_t addr)
{
    return sdtrig_check(addr, SDTRIG_STORE);
}

#endif // CONFIG_ENABLE_ZICSR_EXTENSION
