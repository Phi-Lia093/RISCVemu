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

#ifndef M_EXTENSION_H
#define M_EXTENSION_H

#include <config.h>

#ifdef CONFIG_ENABLE_M_EXTENSION

#include <stdint.h>

void insm_r_mul(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulh(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulsu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_mulu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_div(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_divu(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_rem(uint32_t rs2, uint32_t rs1, uint32_t rd);
void insm_r_remu(uint32_t rs2, uint32_t rs1, uint32_t rd);

#endif // CONFIG_ENABLE_M_EXTENSION

#endif // M_EXTENSION_H
