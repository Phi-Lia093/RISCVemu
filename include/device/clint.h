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

#ifndef CLINT_H
#define CLINT_H

#include <stdbool.h>
#include <stdint.h>

// Core-Local INTerruptor (CLINT). Provides the machine timer used by OpenSBI
// and other SBI firmware:
//   - mtime     (RO) at 0x0200BFF8, a free-running 64-bit counter.
//   - mtimecmp  (RW) at 0x02004000, per-hart compare register (hart 0).
//
// An interrupt is pending (MIP.MTIP) whenever mtime >= mtimecmp.
#define CLINT_BASE 0x02000000u
#define CLINT_SIZE 0x00010000u
#define CLINT_MSIP_HART0 0x02000000u
#define CLINT_MTIMECMP_HART0 0x02004000u
#define CLINT_MTIME_ADDR 0x0200BFF8u

// Reset the CLINT to its power-on state (mtime = 0, mtimecmp = 0, i.e. the
// compare disabled).  Called once at emulator startup so every run begins from
// a deterministic counter value.
void clint_init(void);

// Advance the free-running mtime counter by one tick.
void clint_tick(void);

// Returns true when the machine timer interrupt is pending (mtime >= mtimecmp).
bool clint_mtip_pending(void);

// Return the current value of the free-running mtime counter.  The
// architectural `time` CSR (rdtime) must report the same value that drives
// MIP.MTIP so that firmware can compute an mtimecmp deadline from rdtime and
// have it match the comparator in clint_mtip_pending().
uint64_t clint_get_mtime(void);

// Handle a byte-aligned (or wider, little-endian) MMIO access to the CLINT
// window.  Return the byte value read, or -1 if `addr` is not a register that
// yields a readable byte.  Writes go through clint_store and return -1.
int32_t clint_read(uint32_t addr);
void clint_store(uint32_t addr, uint8_t val);

#endif // CLINT_H
