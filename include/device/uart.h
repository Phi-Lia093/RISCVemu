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

// A 16550-compatible UART for console I/O, mapped at 0x10000000 (the QEMU virt
// UART0 address used by OpenSBI's uart8250 early console and Linux's
// of_serial driver).
//
// Registers (byte-addressed, reg-io-width=1 / reg-shift=0):
//   - THR/RBR (offset 0, DLAB=0): TX write / RX read byte.
//   - IER (offset 1): interrupt-enable register (bit 0 = RX data available).
//   - IIR/FCR (offset 2): FIFO control / interrupt identification.
//   - LCR (offset 3): line control (DLAB bit).
//   - MCR (offset 4), LSR (offset 5), MSR (offset 6), SCR (offset 7).
//
// TX: THR writes are emitted to the host stdout.  LSR reports the
// transmit-holding-register empty (THRE) and transmitter-empty (TEMT) bits so
// polling writes proceed, and the transmitter-empty interrupt (IIR 0x02 /
// IER.ETBEI) is implemented so interrupt-driven tty output drains correctly.
//
// RX: bytes supplied by the host (via uart_receive()/uart_poll_input()) are
// buffered in a small FIFO, surfaced through RBR/LSR.DR/IIR, and drive the
// external interrupt line 10 on the PLIC (when IER.ERBI is set).
#define UART_BASE 0x10000000u
#define UART_NREGS 8u

// External PLIC interrupt line the UART RX path raises.
#define UART_IRQ 10u

// Reset the UART controller (FIFO drained, status cleared, IRQ deasserted).
void uart_init(void);

// Advance the UART transmitter one emulated instruction (pacing the
// transmitter-empty interrupt).  Called once per instruction from the run
// loop, next to clint_tick().
void uart_tick(void);

// Register accessors (byte width, `offset` 0..7).  `uart_read` returns the
// byte value on the 8-byte window; `uart_write` emits/consumes as needed.
uint32_t uart_read(uint32_t offset);
void uart_write(uint32_t offset, uint32_t val);

// Push one host byte into the RX FIFO (from the emulator's input poller).
void uart_receive(uint8_t c);

// Async host-stdin integration (used on POSIX): register stdin for readiness
// notification and drain any available input on each call.  No threads are
// used -- the host kernel (epoll/poll) notifies readiness and we read what is
// available, so there is a single execution context and no re-entrancy.
//
// `raw_mode` (pass 1 in --console mode) hands the host terminal to the guest:
// if stdin is an interactive TTY it is switched to raw mode -- no canonical
// line buffering (keystrokes reach the guest the moment they are typed instead
// of being held until Enter) and no host echo (the guest alone controls the
// screen).  Control characters are passed through to the guest, so to quit
// console mode press Ctrl-] followed by x.  The original terminal settings
// are restored by uart_input_cleanup(), which is also registered via atexit
// and requested from a SIGINT/SIGTERM/SIGQUIT/SIGHUP handler.
void uart_input_setup(int raw_mode);

// Restore the host terminal (if uart_input_setup switched it to raw mode).
void uart_input_cleanup(void);

// Drain any available host input into the RX FIFO.  Returns non-zero when the
// console quit escape (Ctrl-] then x) or a terminating signal was seen, in
// which case the run loop should stop and call uart_input_cleanup().
int uart_poll_input(void);

#endif // CONFIG_ENABLE_UART_DEVICE

#endif // UART_H
