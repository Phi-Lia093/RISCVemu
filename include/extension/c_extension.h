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

#ifndef C_EXTENSION_H
#define C_EXTENSION_H

#include <stdint.h>

#include <config.h>

#ifdef CONFIG_ENABLE_C_EXTENSION

// Decode and execute one 16-bit compressed (RVC) instruction.
//
// Takes ownership of PC advancing for this instruction only:
//   - success + non-control-flow: caller resumes at pc + 2.
//   - success + control-flow:     we set g_state.pc to the branch/jump target
//                                   (the main loop's PC_BACKWARD idiom handles
//                                   the extra 16-bit step).
// Returns 0 on success (PC already adjusted), 1 if a trap was raised
// (illegal instruction / misaligned fetch) and the caller must not advance PC.
int exec_c_insn(uint16_t c_ins);

#endif // CONFIG_ENABLE_C_EXTENSION

#endif // C_EXTENSION_H
