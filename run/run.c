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

/*
 * Fake next-stage kernel (S-mode).
 *
 * OpenSBI's fw_jump payload finishes booting and then hands control to the
 * next boot stage by doing `mret` into S-mode at FW_JUMP_ADDR (0x80400000
 * on this RV32 platform).  This program is that next stage: it proves the
 * whole firmware -> supervisor hand-off works by printing a line to the
 * 16550 UART (0x10000000) and then idling forever, exactly like a real
 * kernel that has finished its early bring-up.
 *
 * It makes no assumptions about the previous stage beyond the architectural
 * ABI: on entry a0 = boot HART id, a1 = device tree pointer (both unused
 * here) and the CPU is already in S-mode with MMU off.
 *
 * It is linked at 0x80400000 (see link_kernel.ld) so the emulator can stage
 * it there via `--load kernel.bin@0x80400000` before OpenSBI jumps to it.
 */

#include <stdint.h>

#define UART_THR 0x10000000UL

/* MMIO console: same 16550 output path the OpenSBI console driver uses. */
static void
putc(char c)
{
    *(volatile uint32_t *)UART_THR = (uint32_t)(uint8_t)c;
}

static void
puts(const char *str)
{
    while (*str) putc(*str++);
}

void _start(void) __attribute__((section(".text._start")));
void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n\t");

    puts("I am a fake kernel\n");

    /* A real kernel would go on to set up MMU, trap handling, a scheduler,
     * drivers, user-space ... ; we simply idle forever. */
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}
