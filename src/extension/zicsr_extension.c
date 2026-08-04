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

#include <config.h>
#include <device/clint.h>
#include <emu.h>
#include <exec.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#include <logger.h>

#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION
#include <extension/zicntr_extension.h>
#endif

#ifdef CONFIG_ENABLE_F_EXTENSION
#include <extension/f_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION
#include <extension/sdtrig_extension.h>
#endif

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

struct csr_operation csr_table[4096] = { 0 };

// ============================================================================
// CSR Callback Functions - Machine Information Registers
// ============================================================================

static uint32_t
get_mvendorid(void)
{
    return 0x0;
}
static uint32_t
get_marchid(void)
{
    return 0x1;
} // RV32G (lower 32 bits)
static uint32_t
get_mimpid(void)
{
    return 0x0;
}
static uint32_t
get_mhartid(void)
{
    return 0;
}

// ============================================================================
// CSR Callback Functions - Machine Trap Setup
// ============================================================================

static uint32_t
get_misa(void)
{
    return csr_table[CSR_MISA].value;
} // RW-ish (WARL)
static uint32_t
get_meideleg(void)
{
    return csr_table[CSR_MEDELEG].value;
}
static uint32_t
get_mideleg(void)
{
    return csr_table[CSR_MIDELEG].value;
}
static uint32_t
get_mie(void)
{
    return csr_table[CSR_MIE].value;
}
static uint32_t
get_mtvec(void)
{
    return csr_table[CSR_MTVEC].value;
}
static uint32_t
get_mcounteren(void)
{
    return csr_table[CSR_MCOUNTEREN].value;
}

// ============================================================================
// CSR Callback Functions - Machine Trap Handling
// ============================================================================

static uint32_t
get_mscratch(void)
{
    return csr_table[CSR_MSCRATCH].value;
}
static uint32_t
get_mepc(void)
{
    return csr_table[CSR_MEPC].value;
}
static uint32_t
get_mcause(void)
{
    return csr_table[CSR_MCAUSE].value;
}
static uint32_t
get_mtval(void)
{
    return csr_table[CSR_MTVAL].value;
}
static uint32_t
get_mip(void)
{
    uint32_t mip = csr_table[CSR_MIP].value;
    // MIP.MTIP is a live view of the CLINT timer: pending whenever
    // mtime >= mtimecmp.  It cannot be cleared by software (only by writing a
    // later mtimecmp), so the stored bit is ignored.
    if (clint_mtip_pending())
    {
        mip |= MIP_MTIP;
    }
    else
    {
        mip &= ~MIP_MTIP;
    }
    return mip;
}
static uint32_t
get_mnstatus(void)
{
    return csr_table[CSR_MNSTATUS].value;
}

// ============================================================================
// CSR Callback Functions - Machine Counters
// ============================================================================

// Note: get_zicntr_* functions are already declared in zicntr_extension.h

// ============================================================================
// CSR Callback Functions - Physical Memory Protection
// ============================================================================

static uint32_t
get_pmpcfg(void)
{
    return csr_table[CSR_PMPCFG0].value;
}
static uint32_t
get_pmpaddr(void)
{
    return csr_table[CSR_PMPADDR0].value;
}
static void
set_pmpcfg(uint32_t val)
{
    csr_table[CSR_PMPCFG0].value = val;
}
static void
set_pmpaddr(uint32_t val)
{
    csr_table[CSR_PMPADDR0].value = val;
}

// ============================================================================
// CSR Callback Functions - Machine Trap Handling (Setters)
// ============================================================================

static void
set_meideleg(uint32_t val)
{
    csr_table[CSR_MEDELEG].value = val;
}
static void
set_mideleg(uint32_t val)
{
    csr_table[CSR_MIDELEG].value = val;
}
static void
set_mie(uint32_t val)
{
    csr_table[CSR_MIE].value = val;
}
static void
set_mtvec(uint32_t val)
{
    csr_table[CSR_MTVEC].value = val;
}
static void
set_mcounteren(uint32_t val)
{
    csr_table[CSR_MCOUNTEREN].value = val;
}

static void
set_mscratch(uint32_t val)
{
    csr_table[CSR_MSCRATCH].value = val;
}
static void
set_mepc(uint32_t val)
{
    csr_table[CSR_MEPC].value = val;
}
static void
set_mcause(uint32_t val)
{
    csr_table[CSR_MCAUSE].value = val;
}
static void
set_mtval(uint32_t val)
{
    csr_table[CSR_MTVAL].value = val;
}
static void
set_mip(uint32_t val)
{
    csr_table[CSR_MIP].value = val;
}

// --- Supervisor interrupt-enable/pending aliasing ---------------------------
// sie/sip are views of the S-relevant bits of mie/mip (SSIP=1, STIP=5,
// SEIP=9).  Writing sie/sip updates the corresponding mie/mip bits, and
// reading returns them.
#define SIP_SIE_S_BITS (MIP_SSIP | MIP_STIP | MIP_SEIP)

static uint32_t
get_sie(void)
{
    return csr_read(CSR_MIE) & SIP_SIE_S_BITS;
}

static void
set_sie(uint32_t val)
{
    uint32_t mie = csr_read(CSR_MIE);
    mie = (mie & ~SIP_SIE_S_BITS) | (val & SIP_SIE_S_BITS);
    csr_write(CSR_MIE, mie);
}

static uint32_t
get_sip(void)
{
    return csr_read(CSR_MIP) & SIP_SIE_S_BITS;
}

static void
set_sip(uint32_t val)
{
    uint32_t mip = csr_read(CSR_MIP);
    mip = (mip & ~SIP_SIE_S_BITS) | (val & SIP_SIE_S_BITS);
    csr_write(CSR_MIP, mip);
}

void
init_csr_table(void)
{
    for (int i = 0; i < 4096; i++)
    {
        csr_table[i].valid = 0;
        csr_table[i].privilege = PRV_MACHINE;
        csr_table[i].rw = RW;
        csr_table[i].value = 0;
        csr_table[i].read_callback = NULL;
        csr_table[i].write_callback = NULL;
    }

    // Sdtrig debug-trigger CSRs (tselect/tdata1/tdata2/tcontrol).
    init_sdtrig_csr_table();

#ifdef CONFIG_ENABLE_F_EXTENSION
    csr_table[CSR_FCSR]
        = (struct csr_operation){ 1, PRV_USER, RW, 0, get_fcsr, set_fcsr };
    csr_table[CSR_FFLAGS]
        = (struct csr_operation){ 1, PRV_USER, RW, 0, get_fflags, set_fflags };
    csr_table[CSR_FRM]
        = (struct csr_operation){ 1, PRV_USER, RW, 0, get_frm, set_frm };
#endif
// Zicntr Extension
#ifdef CONFIG_ENABLE_ZICNTR_EXTENSION
    csr_table[CSR_CYCLE_LO] = (struct csr_operation){
        1, PRV_USER, RO, 0, get_zicntr_cycle_l, NULL
    };
    csr_table[CSR_TIME_LO]
        = (struct csr_operation){ 1, PRV_USER, RO, 0, get_zicntr_time_l, NULL };
    csr_table[CSR_INSTRET_LO] = (struct csr_operation){
        1, PRV_USER, RO, 0, get_zicntr_instret_l, NULL
    };

    csr_table[CSR_CYCLE_HI] = (struct csr_operation){
        1, PRV_USER, RO, 0, get_zicntr_cycle_h, NULL
    };
    csr_table[CSR_TIME_HI]
        = (struct csr_operation){ 1, PRV_USER, RO, 0, get_zicntr_time_h, NULL };
    csr_table[CSR_INSTRET_HI] = (struct csr_operation){
        1, PRV_USER, RO, 0, get_zicntr_instret_h, NULL
    };

#endif

    // Machine Information Registers
    csr_table[CSR_MVENDORID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, get_mvendorid, NULL };
    csr_table[CSR_MARCHID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, get_marchid, NULL };
    csr_table[CSR_MIMPID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, get_mimpid, NULL };
    csr_table[CSR_MHARTID]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, get_mhartid, NULL };
    // mconfigptr: pointer to a config-structure table; advertise 0 (none).
    csr_table[CSR_MCONFIGPTR]
        = (struct csr_operation){ 1, PRV_MACHINE, RO, 0, NULL, NULL };

    // Machine Trap Setup
    csr_table[CSR_MSTATUS]
        = (struct csr_operation){ 1, PRV_MACHINE,  RW,
                                  0, mstatus_read, mstatus_write };
    csr_table[CSR_MISA] = (struct csr_operation){
        // MXL=2 (RV32) | I | M | A | F | C (0x40001125). Reporting these enables
        // the corresponding decode paths and advertises the features an SBI
        // firmware (e.g. OpenSBI) probes (A for atomics, F for the FPU). The
        // extensions are intentionally WARL-masked read-only. S/U are *not*
        // advertised here: the riscv-tests M-mode csr.S gate the user-mode CSR
        // privilege tests on misa.U and the SUPERVISOR/PRIVILEGE enforcement
        // for those windows is intentionally left out of scope for now.
        1, PRV_MACHINE, RO, 0x40001125, get_misa, NULL
    };
    csr_table[CSR_MEDELEG]
        = (struct csr_operation){ 1, PRV_MACHINE,  RW,
                                  0, get_meideleg, set_meideleg };
    csr_table[CSR_MIDELEG]
        = (struct csr_operation){ 1, PRV_MACHINE, RW,
                                  0, get_mideleg, set_mideleg };
    csr_table[CSR_MIE]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mie, set_mie };
    csr_table[CSR_MTVEC]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mtvec, set_mtvec };
    csr_table[CSR_MCOUNTEREN]
        = (struct csr_operation){ 1,          PRV_MACHINE,    RW,
                                  0xFFFFFFFF, get_mcounteren, set_mcounteren };
    // mstatush / menvcfg / menvcfgh / mnscratch / mseccfg / mseccfgh: plain
    // writable machine registers. OpenSBI & other SBI firmware probe/write
    // these during early init (e.g. "csrc mstatush", envcfg setup); without a
    // registration any access raises an illegal-instruction fault.
    csr_table[CSR_MSTATUSH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MENVCFG]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MENVCFGH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MNSCRATCH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MSECCFG]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MSECCFGH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };

    // Machine Trap Handling
    csr_table[CSR_MSCRATCH]
        = (struct csr_operation){ 1, PRV_MACHINE,  RW,
                                  0, get_mscratch, set_mscratch };
    csr_table[CSR_MEPC]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mepc, set_mepc };
    csr_table[CSR_MCAUSE] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_mcause, set_mcause
    };
    csr_table[CSR_MTVAL]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mtval, set_mtval };
    csr_table[CSR_MIP]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mip, set_mip };
    csr_table[CSR_MNSTATUS]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, get_mnstatus, NULL };

    // Machine Counters
    csr_table[CSR_CYCLE_LO]
        = (struct csr_operation){ 1, PRV_MACHINE,        RO,
                                  0, get_zicntr_cycle_l, NULL };
    csr_table[CSR_CYCLE_HI]
        = (struct csr_operation){ 1, PRV_MACHINE,        RO,
                                  0, get_zicntr_cycle_h, NULL };
    csr_table[CSR_TIME_LO] = (struct csr_operation){
        1, PRV_MACHINE, RO, 0, get_zicntr_time_l, NULL
    };
    csr_table[CSR_TIME_HI] = (struct csr_operation){
        1, PRV_MACHINE, RO, 0, get_zicntr_time_h, NULL
    };
    csr_table[CSR_INSTRET_LO]
        = (struct csr_operation){ 1, PRV_MACHINE,          RO,
                                  0, get_zicntr_instret_l, NULL };
    csr_table[CSR_INSTRET_HI]
        = (struct csr_operation){ 1, PRV_MACHINE,          RO,
                                  0, get_zicntr_instret_h, NULL };

    // Writable machine counter/hang CSRs (mcountinhibit, mcycle, minstret)
    // Register as RW so M-mode writes take effect (rv32mi/instret_overflow.S).
    csr_table[CSR_MCOUNTINHIBIT]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MCYCLE]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    csr_table[CSR_MCYCLEH]
        = (struct csr_operation){ 1, PRV_MACHINE, RW, 0, NULL, NULL };
    // minstret/minstreth: writable aliases of the live instret counter. Writing
    // suppresses the architecturally-incrementing counter for that instruction.
    csr_table[CSR_MINSTRET] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_zicntr_instret_l, set_zicntr_minstret_l
    };
    csr_table[CSR_MINSTRETH] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_zicntr_instret_h, set_zicntr_minstret_h
    };

    // Physical Memory Protection
    csr_table[CSR_PMPCFG0] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_pmpcfg, set_pmpcfg
    };
    csr_table[CSR_PMPCFG1] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_pmpcfg, set_pmpcfg
    };
    csr_table[CSR_PMPCFG2] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_pmpcfg, set_pmpcfg
    };
    csr_table[CSR_PMPCFG3] = (struct csr_operation){
        1, PRV_MACHINE, RW, 0, get_pmpcfg, set_pmpcfg
    };
    for (int i = 0; i < 16; i++)
    {
        csr_table[CSR_PMPADDR0 + i]
            = (struct csr_operation){ 1, PRV_MACHINE, RW,
                                      0, get_pmpaddr, set_pmpaddr };
    }

    // Supervisor Trap Setup
    csr_table[CSR_SSTATUS]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW,
                                  0, sstatus_read,   sstatus_write };
    csr_table[CSR_SIE]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, get_sie, set_sie };
    csr_table[CSR_STVEC]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SSCRATCH]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SEPC]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SCAUSE]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_STVAL]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SIP]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, get_sip, set_sip };
    csr_table[CSR_SATP]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    // scounteren (0x106): S-mode counter enable; a plain RW register suffices.
    csr_table[0x106]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    // senvcfg / senvcfgh: S-mode environment configuration; plain RW registers.
    csr_table[CSR_SENVCFG]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
    csr_table[CSR_SENVCFGH]
        = (struct csr_operation){ 1, PRV_SUPERVISOR, RW, 0, NULL, NULL };
}

void
ins_zicsr_csrrw(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    uint32_t new_val = reg_read(rs1);
    csr_write(csr, new_val);
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrs(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    if (rs1 != 0)
    {
        uint32_t rs1_val = reg_read(rs1);
        uint32_t new_val = old_val | rs1_val;
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrc(uint32_t rs1, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    if (rs1 != 0)
    {
        uint32_t rs1_val = reg_read(rs1);
        uint32_t new_val = old_val & ~rs1_val;
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrwi(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    uint32_t new_val = uimm & 0x1F;
    csr_write(csr, new_val);
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrsi(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    if (uimm != 0)
    {
        uint32_t new_val = old_val | (uimm & 0x1F);
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

void
ins_zicsr_csrrci(uint32_t uimm, uint32_t rd, uint32_t csr, uint32_t ins)
{
    check_csr_access(csr, ins);
    uint32_t old_val = csr_read(csr);
    if (uimm != 0)
    {
        uint32_t new_val = old_val & ~(uimm & 0x1F);
        csr_write(csr, new_val);
    }
    if (rd != 0)
    {
        reg_write(rd, old_val);
    }
}

#endif // CONFIG_ENABLE_ZICSR_EXTENSION
