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

#ifndef MMU_H
#define MMU_H

#include <config.h>

#ifdef CONFIG_ENABLE_ZICSR_EXTENSION

#include <emu.h>
#include <extension/system.h>
#include <extension/zicsr_extension.h>
#include <mem.h>
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Supervisor virtual-memory support (SATP / SV32).
//
// Address translation is applied to a memory access only when the *effective*
// privilege of that access is Supervisor or User, satp.MODE is set (SV32 for
// RV32), and the access is not deliberately left physical.  Machine-mode
// accesses and accesses made while the effective privilege is Machine bypass
// translation entirely (they are physical).
//
// The effective privilege of data loads/stores is mstatus.MPP when
// mstatus.MPRV is set (else the current privilege).  Instruction fetches are
// never affected by MPRV/MPP -- they always use the current privilege.
//
// Page faults are raised per-access with the correct cause and mtval/stval
// set to the virtual address.  A/D page-table bits are *not* maintained
// automatically (matching the riscv-tests dirty.S expectation that D is not
// set in hardware).
// ============================================================================

#define SATP32_MODE_BIT 0x80000000u
#define SATP32_PPN_MASK 0x003FFFFFu

#define PTE_V 0x001u
#define PTE_R 0x002u
#define PTE_W 0x004u
#define PTE_X 0x008u
#define PTE_U 0x010u
#define PTE_A 0x040u
#define PTE_D 0x080u
#define PTE_PPN_SHIFT 10

#define PGSHIFT 12
#define PGSIZE (1u << PGSHIFT)

// --- access direction / size, used to select the correct page-fault cause ---
enum mmu_access
{
    MMU_FETCH = 0,
    MMU_LOAD = 1,
    MMU_STORE = 2
};

static inline uint32_t
mmu_satp_mode(void)
{
    // RV32 satp: MODE is bit 31; mode 1 == SV32.
    uint32_t satp = csr_read(CSR_SATP);
    return (satp & SATP32_MODE_BIT) ? 1 : 0;
}

// Effective privilege for data loads/stores (MPRV applies), never fetch.
static inline uint32_t
mmu_effective_dpriv(void)
{
    uint32_t mstatus = csr_read(CSR_MSTATUS);
    if (mstatus & MSTATUS_MPRV)
    {
        return (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
    }
    return g_state.privilege;
}

// True when an access with this effective privilege must be translated.
static inline bool
mmu_vm_active(uint32_t epriv)
{
    if (epriv == PRV_MACHINE) return false; // Machine never translates.
    return mmu_satp_mode() != 0;            // SV32 active.
}

// Root page-table physical-page number from satp (RV32 SV32 field).
static inline uint32_t
mmu_satp_root_ppn(void)
{
    return csr_read(CSR_SATP) & SATP32_PPN_MASK;
}

// Permission checks on a resolved (leaf) PTE. Returns false (fault) if the leaf
// does not grant the needed R/W/X and privilege (U / SUM / MXR).
static inline bool
mmu_check_pte(uint32_t pte, uint32_t epriv, bool write, enum mmu_access access)
{
    bool user_leaf = (pte & PTE_U) != 0;
    if (epriv == PRV_MACHINE)
    {
        return true; // Machine bypasses (PTE V already checked by caller).
    }
    // A page must be marked Accessed before any access (the implementation does
    // not maintain A/D automatically, so clear A faults).
    if ((pte & PTE_A) == 0) return false;
    if (write)
    {
        if ((pte & PTE_W) == 0) return false; // write requires W
        // A store to a leaf with D clear raises a store page fault (D is not
        // set automatically; the OS handler sets it and retries).
        if ((pte & PTE_D) == 0) return false;
    }
    else if (access == MMU_FETCH)
    {
        if ((pte & PTE_X) == 0) return false; // fetch requires X
    }
    else
    {
        // load: requires R, or X if MXR is set.
        if ((pte & PTE_R) == 0 && (pte & PTE_X) == 0) return false;
        if ((pte & PTE_R) == 0 && (pte & PTE_X) != 0)
        {
            uint32_t mstatus = csr_read(CSR_MSTATUS);
            if (!(mstatus & MSTATUS_MXR)) return false;
        }
    }

    if (epriv == PRV_SUPERVISOR)
    {
        if (user_leaf)
        {
            uint32_t mstatus = csr_read(CSR_MSTATUS);
            if (!(mstatus & MSTATUS_SUM)) return false; // needs SUM for U mem
        }
    }
    else if (epriv == PRV_USER)
    {
        if (!user_leaf) return false; // U can only access U pages.
    }
    return true;
}

// Translate `vaddr` to a physical address. Returns 0 and sets *ppa on success,
// or raises the appropriate page-fault exception and returns nonzero.
static inline int
mmu_translate(uint32_t vaddr, enum mmu_access access, uint32_t *ppa)
{
    bool write = (access == MMU_STORE);
    uint32_t epriv
        = (access == MMU_FETCH) ? g_state.privilege : mmu_effective_dpriv();

    if (!mmu_vm_active(epriv))
    {
        *ppa = vaddr;
        return 0;
    }

    uint32_t root = mmu_satp_root_ppn() << PGSHIFT;
    uint32_t vpn1 = (vaddr >> 22) & 0x3FFu;
    uint32_t vpn0 = (vaddr >> 12) & 0x3FFu;

    // Level-1 (page directory) entry.
    uint32_t pte1 = mem_read32_unsigned(root + vpn1 * 4);
    if ((pte1 & PTE_V) == 0) goto fault;

    // If the level-1 entry is a leaf (superpage, 4 MiB), check it directly.
    if ((pte1 & (PTE_R | PTE_W | PTE_X)) != 0)
    {
        // Superpage leaves require the PPN to be aligned to the superpage size
        // (1024 pages); a set low PPN bit is a misaligned-superpage page fault.
        if (((pte1 >> PTE_PPN_SHIFT) & 0x3FFu) != 0u) goto fault;
        if (!mmu_check_pte(pte1, epriv, write, access)) goto fault;
        *ppa = ((pte1 >> PTE_PPN_SHIFT) << PGSHIFT) | (vaddr & 0x3FFFFFu);
        return 0;
    }

    // Level-1 entry is a pointer to a level-2 table.
    uint32_t level2_base = (pte1 >> PTE_PPN_SHIFT) << PGSHIFT;
    uint32_t pte2 = mem_read32_unsigned(level2_base + vpn0 * 4);
    if ((pte2 & PTE_V) == 0) goto fault;
    if ((pte2 & (PTE_R | PTE_W | PTE_X)) == 0) goto fault; // table, not leaf
    if (!mmu_check_pte(pte2, epriv, write, access)) goto fault;
    *ppa = ((pte2 >> PTE_PPN_SHIFT) << PGSHIFT) | (vaddr & 0xFFFu);
    return 0;

fault:
    if (access == MMU_FETCH)
        raise_exception(CAUSE_FETCH_PAGE_FAULT, vaddr);
    else if (access == MMU_STORE)
        raise_exception(CAUSE_STORE_PAGE_FAULT, vaddr);
    else
        raise_exception(CAUSE_LOAD_PAGE_FAULT, vaddr);
    return 1;
}

// ---- virtual read/write helpers (translate then physical access) ----
// Fetch helper: translate the instruction-fetch `vaddr`, fetch one 32-bit
// word, and store it to *ins.  Returns 1 on success, or 0 after raising a
// fetch page-fault (caller must not execute the instruction).
static inline int
mmu_fetch_ok(uint32_t vaddr, uint32_t *ins)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_FETCH, &pa)) return 0;
    *ins = mem_read32_unsigned(pa);
    return 1;
}

static inline uint32_t
mmu_read8_unsigned(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read8_unsigned(pa);
}

static inline int32_t
mmu_read8_signed(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read8_signed(pa);
}

static inline uint32_t
mmu_read16_unsigned(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read16_unsigned(pa);
}

static inline int32_t
mmu_read16_signed(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read16_signed(pa);
}

static inline uint32_t
mmu_read32_unsigned(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read32_unsigned(pa);
}

static inline int32_t
mmu_read32_signed(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read32_signed(pa);
}

static inline uint64_t
mmu_read64_unsigned(uint32_t vaddr)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_LOAD, &pa)) return 0;
    return mem_read64_unsigned(pa);
}

static inline void
mmu_write8(uint32_t vaddr, uint8_t val)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_STORE, &pa)) return;
    mem_write8(pa, val);
}

static inline void
mmu_write16(uint32_t vaddr, uint16_t val)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_STORE, &pa)) return;
    mem_write16(pa, val);
}

static inline void
mmu_write32(uint32_t vaddr, uint32_t val)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_STORE, &pa)) return;
    mem_write32(pa, val);
}

static inline void
mmu_write64(uint32_t vaddr, uint64_t val)
{
    uint32_t pa;
    if (mmu_translate(vaddr, MMU_STORE, &pa)) return;
    mem_write64(pa, val);
}

#endif // CONFIG_ENABLE_ZICSR_EXTENSION

#endif // MMU_H
