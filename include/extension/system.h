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

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#define MSTATUS_UIE (1 << 0)
#define MSTATUS_SIE (1 << 1)
#define MSTATUS_MIE (1 << 3)
#define MSTATUS_UPIE (1 << 4)
#define MSTATUS_SPIE (1 << 5)
#define MSTATUS_MPIE (1 << 7)
#define MSTATUS_SPP (1 << 8)
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_MPP_MASK (3 << MSTATUS_MPP_SHIFT)
#define MSTATUS_FS_SHIFT 13
#define MSTATUS_FS_MASK (3 << MSTATUS_FS_SHIFT)
#define MSTATUS_XS_SHIFT 15
#define MSTATUS_XS_MASK (3 << MSTATUS_XS_SHIFT)
#define MSTATUS_MPRV (1 << 17)
#define MSTATUS_SUM (1 << 18)
#define MSTATUS_MXR (1 << 19)
#define MSTATUS_TVM (1 << 20)
#define MSTATUS_TW (1 << 21)
#define MSTATUS_TSR (1 << 22)
#define MSTATUS_UXL_SHIFT 32
#define MSTATUS_SXL_SHIFT 34

#define MIP_USIP (1 << 0)
#define MIP_SSIP (1 << 1)
#define MIP_MSIP (1 << 3)
#define MIP_UTIP (1 << 4)
#define MIP_STIP (1 << 5)
#define MIP_MTIP (1 << 7)
#define MIP_UEIP (1 << 8)
#define MIP_SEIP (1 << 9)
#define MIP_MEIP (1 << 11)

void ins_sret(void);
void ins_mret(void);
void ins_mnret(void);
void ins_wfi(void);
void ins_sctrclr(void);
void ins_sfence_vma(void);

uint32_t mstatus_read(void);
void mstatus_write(uint32_t val);
uint32_t sstatus_read(void);
void sstatus_write(uint32_t val);

void raise_exception(uint32_t cause, uint32_t tval);
void raise_supervisor_interrupt(uint32_t irq);
void raise_interrupt(uint32_t irq);
void handle_trap(void);
void check_and_handle_interrupts(void);

#define CAUSE_MISALIGNED_FETCH 0
#define CAUSE_FAULT_FETCH 1
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT 3
#define CAUSE_MISALIGNED_LOAD 4
#define CAUSE_FAULT_LOAD 5
#define CAUSE_MISALIGNED_STORE 6
#define CAUSE_FAULT_STORE 7
#define CAUSE_USER_ECALL 8
#define CAUSE_SUPERVISOR_ECALL 9
#define CAUSE_HYPERVISOR_ECALL 10
#define CAUSE_MACHINE_ECALL 11
#define CAUSE_FETCH_PAGE_FAULT 12
#define CAUSE_LOAD_PAGE_FAULT 13
#define CAUSE_STORE_PAGE_FAULT 15

#endif
