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

#ifndef ZICNTR_EXTENSION_H
#define ZICNTR_EXTENSION_H

#include <config.h>

#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION

#include <stdint.h>

extern uint64_t cycle;

#define CSR_CYCLE_LO 0xC00
#define CSR_TIME_LO 0xC01
#define CSR_INSTRET_LO 0xC02

#define CSR_CYCLE_HI 0xC80
#define CSR_TIME_HI 0xC81
#define CSR_INSTRET_HI 0xC82

uint32_t get_zicntr_cycle_l();
uint32_t get_zicntr_cycle_h();
uint32_t get_zicntr_time_l();
uint32_t get_zicntr_time_h();

#endif // CONFIG_ENABLE_ZICNTR_EXTENSION

#endif /* ZICNTR_EXTENSION_H */
