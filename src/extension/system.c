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

#include <emu.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#include <logger.h>

uint32_t
mstatus_read(void)
{
    return csr_table[CSR_MSTATUS].value;
}

void
mstatus_write(uint32_t val)
{
    const uint32_t writable_mask
        = MSTATUS_UIE | MSTATUS_SIE | MSTATUS_MIE | MSTATUS_UPIE | MSTATUS_SPIE
          | MSTATUS_MPIE | MSTATUS_SPP | MSTATUS_MPP_MASK | MSTATUS_FS_MASK
          | MSTATUS_XS_MASK | MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR
          | MSTATUS_TVM | MSTATUS_TW | MSTATUS_TSR;
    csr_table[CSR_MSTATUS].value = val & writable_mask;
}

uint32_t
sstatus_read(void)
{
    uint32_t m = csr_table[CSR_MSTATUS].value;
    return ((m & MSTATUS_UIE) ? 1 : 0) | ((m & MSTATUS_SIE) ? 2 : 0)
           | ((m & MSTATUS_UPIE) ? 16 : 0) | ((m & MSTATUS_SPIE) ? 32 : 0)
           | ((m & MSTATUS_SPP) ? 256 : 0)
           | (((m & MSTATUS_FS_MASK) >> MSTATUS_FS_SHIFT) << 13)
           | (((m & MSTATUS_XS_MASK) >> MSTATUS_XS_SHIFT) << 15)
           | ((m & MSTATUS_SUM) ? (1 << 18) : 0)
           | ((m & MSTATUS_MXR) ? (1 << 19) : 0);
}

void
sstatus_write(uint32_t val)
{
    uint32_t m = csr_table[CSR_MSTATUS].value;
    const uint32_t s_mask = MSTATUS_UIE | MSTATUS_SIE | MSTATUS_UPIE
                            | MSTATUS_SPIE | MSTATUS_SPP | MSTATUS_FS_MASK
                            | MSTATUS_XS_MASK | MSTATUS_SUM | MSTATUS_MXR;

    m &= ~s_mask;
    if (val & 1) m |= MSTATUS_UIE;
    if (val & 2) m |= MSTATUS_SIE;
    if (val & 16) m |= MSTATUS_UPIE;
    if (val & 32) m |= MSTATUS_SPIE;
    if (val & 256) m |= MSTATUS_SPP;
    m |= ((val >> 13) & 3) << MSTATUS_FS_SHIFT;
    m |= ((val >> 15) & 3) << MSTATUS_XS_SHIFT;
    if (val & (1 << 18)) m |= MSTATUS_SUM;
    if (val & (1 << 19)) m |= MSTATUS_MXR;
    csr_table[CSR_MSTATUS].value = m;
}

void
ins_mret(void)
{
    uint32_t mstatus_val = csr_table[CSR_MSTATUS].value;
    uint32_t mpp = (mstatus_val & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;

    if (mstatus_val & MSTATUS_MPIE)
    {
        mstatus_val |= MSTATUS_MIE;
    }
    else
    {
        mstatus_val &= ~MSTATUS_MIE;
    }

    mstatus_val = (mstatus_val | MSTATUS_MPIE) & ~MSTATUS_MPP_MASK;
    mstatus_val |= (PRV_USER << MSTATUS_MPP_SHIFT);

    csr_table[CSR_MSTATUS].value = mstatus_val;
    g_state.privilege = mpp;
    g_state.pc = csr_table[CSR_MEPC].value - 4;
}

void
ins_sret(void)
{
    uint32_t sstatus_val = csr_table[CSR_SSTATUS].value;
    uint32_t spp = (sstatus_val >> 8) & 1;

    if (sstatus_val & (1 << 5))
    {
        sstatus_val |= (1 << 1);
    }
    else
    {
        sstatus_val &= ~(1 << 1);
    }

    sstatus_val = (sstatus_val | (1 << 5)) & ~(1 << 8);
    csr_table[CSR_SSTATUS].value = sstatus_val;

    g_state.privilege = spp ? PRV_SUPERVISOR : PRV_USER;
    g_state.pc = csr_table[CSR_SEPC].value - 4;
}

void
ins_mnret(void)
{
    fatal("mnret not implemented");
}

void
ins_wfi(void)
{
    debug("WFI executed, ignoring");
}

void
ins_sctrclr(void)
{
    fatal("sctrclr not implemented");
}

void
ins_sfence_vma(void)
{
    debug("SFENCE.VMA executed, ignoring");
}

static void raise_machine_exception(uint32_t cause, uint32_t tval, uint32_t pc);
static void raise_supervisor_exception(uint32_t cause, uint32_t tval,
                                       uint32_t pc);
static void raise_machine_interrupt(uint32_t irq, uint32_t pc);
static void raise_supervisor_interrupt_internal(uint32_t irq, uint32_t pc);

void
raise_exception(uint32_t cause, uint32_t tval)
{
    uint32_t current_privilege = g_state.privilege;
    uint32_t pc = g_state.pc;

    warn("Exception: cause=%d, tval=0x%08X, pc=0x%08X, priv=%d", cause, tval,
         pc, current_privilege);

    bool delegate_to_s = false;

    if (current_privilege == PRV_USER)
    {
        uint32_t medeleg = csr_read(CSR_MEDELEG);
        delegate_to_s = (medeleg & (1 << cause)) != 0;
    }
    else if (current_privilege == PRV_SUPERVISOR)
    {
        uint32_t medeleg = csr_read(CSR_MEDELEG);
        delegate_to_s = (medeleg & (1 << cause)) == 0;
    }
    else
    {
        delegate_to_s = false;
    }

    if (delegate_to_s)
    {
        raise_supervisor_exception(cause, tval, pc);
    }
    else
    {
        raise_machine_exception(cause, tval, pc);
    }
}

static void
raise_machine_exception(uint32_t cause, uint32_t tval, uint32_t pc)
{
    uint32_t mstatus = csr_read(CSR_MSTATUS);

    mstatus = (mstatus & ~MSTATUS_MPP_MASK)
              | (g_state.privilege << MSTATUS_MPP_SHIFT);

    if (mstatus & MSTATUS_MIE)
    {
        mstatus |= MSTATUS_MPIE;
    }
    else
    {
        mstatus &= ~MSTATUS_MPIE;
    }
    mstatus &= ~MSTATUS_MIE;
    csr_write(CSR_MSTATUS, mstatus);

    csr_write(CSR_MCAUSE, cause);
    csr_write(CSR_MTVAL, tval);
    csr_write(CSR_MEPC, pc);

    g_state.privilege = PRV_MACHINE;

    uint32_t mtvec = csr_read(CSR_MTVEC);
    g_state.pc = (mtvec & ~0x3) - 4;
}

static void
raise_supervisor_exception(uint32_t cause, uint32_t tval, uint32_t pc)
{
    uint32_t sstatus = csr_read(CSR_SSTATUS);

    if (g_state.privilege == PRV_SUPERVISOR)
    {
        sstatus |= (1 << 8);
    }
    else
    {
        sstatus &= ~(1 << 8);
    }

    if (sstatus & (1 << 1))
    {
        sstatus |= (1 << 5);
    }
    else
    {
        sstatus &= ~(1 << 5);
    }
    sstatus &= ~(1 << 1);
    csr_write(CSR_SSTATUS, sstatus);

    csr_write(CSR_SCAUSE, cause);
    csr_write(CSR_STVAL, tval);
    csr_write(CSR_SEPC, pc);

    g_state.privilege = PRV_SUPERVISOR;

    uint32_t stvec = csr_read(CSR_STVEC);
    g_state.pc = (stvec & ~0x3) - 4;
}

void
check_and_handle_interrupts(void)
{
    uint32_t mip = csr_read(CSR_MIP);
    uint32_t mie = csr_read(CSR_MIE);
    uint32_t mstatus = csr_read(CSR_MSTATUS);
    uint32_t sip = csr_read(CSR_SIP);
    uint32_t sie = csr_read(CSR_SIE);
    uint32_t sstatus = csr_read(CSR_SSTATUS);

    if (mstatus & MSTATUS_MIE)
    {
        uint32_t pending = mip & mie;
        if (pending)
        {
            int irq = -1;
            if (pending & MIP_MEIP)
                irq = 11;
            else if (pending & MIP_MSIP)
                irq = 3;
            else if (pending & MIP_MTIP)
                irq = 7;
            else if (pending & MIP_SEIP)
                irq = 9;
            else if (pending & MIP_SSIP)
                irq = 1;
            else if (pending & MIP_STIP)
                irq = 5;

            if (irq >= 0)
            {
                debug("Taking M-mode interrupt: irq=%d", irq);
                raise_interrupt(irq);
                return;
            }
        }
    }

    if (sstatus & (1 << 1))
    {
        uint32_t pending = sip & sie;
        if (pending)
        {
            uint32_t mideleg = csr_read(CSR_MIDELEG);
            int irq = -1;

            if ((pending & MIP_SEIP) && (mideleg & MIP_SEIP))
                irq = 9;
            else if ((pending & MIP_SSIP) && (mideleg & MIP_SSIP))
                irq = 1;
            else if ((pending & MIP_STIP) && (mideleg & MIP_STIP))
                irq = 5;

            if (irq >= 0)
            {
                debug("Taking S-mode interrupt: irq=%d", irq);
                raise_interrupt(irq);
                return;
            }
        }
    }
}

void
raise_interrupt(uint32_t irq)
{
    uint32_t current_privilege = g_state.privilege;
    uint32_t pc = g_state.pc;

    debug("Interrupt: irq=%d, pc=0x%08X, priv=%d", irq, pc, current_privilege);

    bool delegate_to_s = false;
    uint32_t mideleg = csr_read(CSR_MIDELEG);

    if (current_privilege == PRV_MACHINE)
    {
        delegate_to_s = (mideleg & (1 << irq)) != 0;
    }
    else if (current_privilege == PRV_SUPERVISOR)
    {
        delegate_to_s = true;
    }
    else
    {
        delegate_to_s = (mideleg & (1 << irq)) != 0;
    }

    if (delegate_to_s)
    {
        raise_supervisor_interrupt_internal(irq, pc);
    }
    else
    {
        raise_machine_interrupt(irq, pc);
    }
}

static void
raise_machine_interrupt(uint32_t irq, uint32_t pc)
{
    uint32_t cause = irq | (1 << 31);
    uint32_t mstatus = csr_read(CSR_MSTATUS);

    mstatus = (mstatus & ~MSTATUS_MPP_MASK)
              | (g_state.privilege << MSTATUS_MPP_SHIFT);

    if (mstatus & MSTATUS_MIE)
    {
        mstatus |= MSTATUS_MPIE;
    }
    else
    {
        mstatus &= ~MSTATUS_MPIE;
    }
    mstatus &= ~MSTATUS_MIE;
    csr_write(CSR_MSTATUS, mstatus);

    csr_write(CSR_MCAUSE, cause);
    csr_write(CSR_MTVAL, 0);
    csr_write(CSR_MEPC, pc);

    g_state.privilege = PRV_MACHINE;

    uint32_t mtvec = csr_read(CSR_MTVEC);
    g_state.pc = (mtvec & ~0x3) - 4;
}

static void
raise_supervisor_interrupt_internal(uint32_t irq, uint32_t pc)
{
    uint32_t cause = irq | (1 << 31);
    uint32_t sstatus = csr_read(CSR_SSTATUS);

    if (g_state.privilege == PRV_SUPERVISOR)
    {
        sstatus |= (1 << 8);
    }
    else
    {
        sstatus &= ~(1 << 8);
    }

    if (sstatus & (1 << 1))
    {
        sstatus |= (1 << 5);
    }
    else
    {
        sstatus &= ~(1 << 5);
    }
    sstatus &= ~(1 << 1);
    csr_write(CSR_SSTATUS, sstatus);

    csr_write(CSR_SCAUSE, cause);
    csr_write(CSR_STVAL, 0);
    csr_write(CSR_SEPC, pc);

    g_state.privilege = PRV_SUPERVISOR;

    uint32_t stvec = csr_read(CSR_STVEC);
    g_state.pc = (stvec & ~0x3) - 4;
}

void
raise_supervisor_interrupt(uint32_t irq)
{
    raise_supervisor_interrupt_internal(irq, g_state.pc);
}
