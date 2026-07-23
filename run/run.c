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

void
_start(void) __attribute__((section(".text._start")));

int
_main();

void debug_print(char *str)
{
    while (*str) {
        *(volatile unsigned int *)0x10000000 = *str++;
    }
}

void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n");
    __asm__ volatile("wfi\n");
    _main();
    __asm__ volatile("ecall\n");
    while (1);
}

int
_main()
{
    debug_print("Hello, World!\n");
    return 0x325;
}
