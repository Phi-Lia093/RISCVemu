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

#ifndef SDTRIG_EXTENSION_H
#define SDTRIG_EXTENSION_H

#include <config.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

#include <stdint.h>

// Sdtrig (trigger) registers. These make the rv32mi breakpoint test pass: the
// test programs MCONTROL triggers (tdata1/tdata2) and expects hardware
// breakpoints on execute / load / store accesses to raise CAUSE_BREAKPOINT.
//
// tdata1 bit layout (RV32):
//   [31:28] type   [27] dmode [26:21] maskmax [20] hit [19] select [18] timing
//   [17:12] action [11] chain [10:7] match [6] M [5] H [4] S [3] U
//   [2] execute [1] store [0] load
#define SDTRIG_TYPE_SHIFT 28
#define SDTRIG_DMODE (1UL << 27)
#define SDTRIG_ACTION_SHIFT 12
#define SDTRIG_ACTION_MASK (0x3FUL << SDTRIG_ACTION_SHIFT)
#define SDTRIG_CHAIN (1UL << 11)
#define SDTRIG_MATCH_SHIFT 7
#define SDTRIG_MATCH_MASK (0x0FUL << SDTRIG_MATCH_SHIFT)
#define SDTRIG_MODE_M (1UL << 6)
#define SDTRIG_MODE_H (1UL << 5)
#define SDTRIG_MODE_S (1UL << 4)
#define SDTRIG_MODE_U (1UL << 3)
#define SDTRIG_EXECUTE (1UL << 2)
#define SDTRIG_STORE (1UL << 1)
#define SDTRIG_LOAD (1UL << 0)

#define SDTRIG_TYPE_MATCH 2
#define SDTRIG_ACTION_DEBUG_EXCEPTION 0

// tcontrol.mte: enabling bit written by riscv-tests rv32mi/breakpoint.
#define SDTRIG_TCONTROL_MTE 0x8

// Number of trigger slots addressable via tselect.
#define SDTRIG_NUM_TRIGGERS 4

// Registration of the Sdtrig CSRs into the global CSR table.
void init_sdtrig_csr_table(void);

// Trigger checking helpers. Each returns nonzero if a breakpoint trap was
// raised (in which case the memory access / fetch must be suppressed).
int sdtrig_check_fetch_trigger(uint32_t pc);
int sdtrig_check_load_trigger(uint32_t addr);
int sdtrig_check_store_trigger(uint32_t addr);

#endif // CONFIG_ENABLE_ZICSR_EXTENSION
#endif // SDTRIG_EXTENSION_H
