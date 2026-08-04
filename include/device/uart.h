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

#ifndef UART_H
#define UART_H

#include <config.h>

#ifdef CONFIG_ENABLE_UART_DEVICE

#include <stdint.h>
#include <stdio.h>

// A minimal 16550-compatible UART for console output, mapped at 0x10000000
// (the QEMU/LiteX virt UART0 address used by OpenSBI's sifive-uart/uart16550
// early console).  Only output is modeled:
//   - THR (offset 0, DLAB=0) write emits the byte.
//   - LSR (offset 5) reports the transmit-holding-register empty (THRE) and
//     transmitter-empty (TEMT) bits so firmware polling writes proceed.
//   - IIR (offset 2) reports "no interrupt pending" to avoid undefined state.
//   - LCR/divisor registers are stored so DLAB/baud writes do not trap.
#define UART_BASE 0x10000000u
#define UART_NREGS 8u

static inline uint32_t
uart_read(uint32_t offset)
{
    switch (offset & 0x7u)
    {
    case 0: // RBR/THR, DLL (no buffered input -> 0)
    case 1: // IER/DLH
        return 0;
    case 2: // IIR: no interrupt pending
        return 0x01;
    case 3: // LCR
        return 0;
    case 4: // MCR
        return 0;
    case 5: // LSR: THRE | TEMT
        return 0x60;
    case 6: // MSR
        return 0;
    default: // SCR
        return 0;
    }
}

static inline void
uart_write(uint32_t offset, uint32_t val)
{
    // THR (offset 0) with DLAB clear: transmit a character.
    if ((offset & 0x7u) == 0)
    {
        putc((int)(val & 0xFFu), stdout);
        fflush(stdout);
    }
    // All other register writes (LER, FCR, MCR, LCR, divisor) are no-ops that
    // just need to be accepted so firmware does not trap.
}

#endif // CONFIG_ENABLE_UART_DEVICE

#endif // UART_H
