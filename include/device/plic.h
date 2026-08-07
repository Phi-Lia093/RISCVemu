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

#ifndef PLIC_H
#define PLIC_H

#include <stdbool.h>
#include <stdint.h>

// Platform-Level Interrupt Controller (PLIC), modelled on the single-hart
// QEMU "virt" PLIC so that Linux's riscv,plic0 driver and OpenSBI's PLIC
// driver both work unmodified.
//
//   - priority[i]        (RW) : base + 0x000000 + 4*i    (i = 1..ndev)
//   - pending            (RO) : base + 0x001000 + 4*(id/32)
//   - enable[ctx]        (RW) : base + 0x002000 + 0x80*ctx + 4*(id/32)
//   - threshold[ctx]     (RW) : base + 0x200000 + 0x1000*ctx
//   - claim/complete[ctx](RW) : base + 0x200004 + 0x1000*ctx
//
// Two hart contexts are modelled: context 0 is the machine-mode context used
// by OpenSBI, context 1 is the supervisor-mode context used by Linux (the DT
// advertises interrupts-extended = <&cpu_intc 11> <&cpu_intc 9>, so the S
// entry maps to context 1).  The "external interrupt" lines are driven as:
//   - MIP.MEIP while the highest-priority enabled M-mode source is pending
//   - MIP.SEIP while the highest-priority enabled S-mode source is pending
// which is how the emulator's get_mip()/check_and_handle_interrupts() sees
// them.  Sources are 1-based (0 is reserved).
#define PLIC_BASE 0x0C000000u
#define PLIC_SIZE 0x04000000u

// Total number of externally-mapped interrupt sources (1-based, max irq id).
// The UART uses line 10 (QEMU virt convention); ndev = highest used id.
#define PLIC_NDEV 10u

// MMIO register helpers.  Handled at 32-bit granularity by mem.h (word
// reads/writes).  Byte reads/writes are expanded by mem.h into the CLINT
// accessors, so these only need to cope with 32-bit addresses inside the
// PLIC window.
uint32_t plic_read32(uint32_t off);
void plic_write32(uint32_t off, uint32_t val);

// Reset the PLIC to a quiescent state.
void plic_init(void);

// Assert (level != 0) or deassert (level == 0) external interrupt source `id`.
// Called by peripheral models (e.g. the UART) when a device interrupt line
// changes.  `id` must be in [1, PLIC_NDEV].
void plic_set_irq(unsigned id, bool level);

// Highest-priority enabled-and-pending source for the M-mode context (0) or
// the S-mode context (1), or 0 if none.  Used to decide whether MIP.MEIP /
// MIP.SEIP is asserted.
unsigned plic_external_pending(void);
unsigned plic_external_pending_s(void);
unsigned plic_irq_count(void);

#endif // PLIC_H
