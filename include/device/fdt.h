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

#ifndef FDT_H
#define FDT_H

#include <config.h>

#ifdef CONFIG_ENABLE_FDT

#include <stddef.h>
#include <stdint.h>

/*
 * Flattened Device Tree (FDT) support.
 *
 * RISC-V firmware such as OpenSBI follows the SBI device-tree passing
 * convention: when it is jumped to, register a1 must point at a valid,
 * big-endian FDT (and a0 must hold the boot HART id, normally 0).  Without
 * this OpenSBI's generic platform init (fw_platform_init) fails to locate the
 * "/" and "/cpus" nodes and hangs in `while (1) wfi();`.
 *
 * This module builds a minimal FDT from scratch (no libfdt/dtc dependency) and
 * describes the devices modelled by the emulator.  It exposes a small, generic
 * write-only "builder" API so additional nodes/properties can be added easily
 * when the emulator gains new devices.
 *
 * The FDT is built directly into the emulated RAM at a reserved address and the
 * boot registers are set before the firmware begins execution.
 *
 * Default FDT binary address.  OpenSBI/QEMU convention places the DTB just
 * below the top of physical RAM; for this emulator's RV32 4 GiB map that is
 * 0x87f00000 (well above the 0x80000000 fw_jump load base plus firmware size).
 */
#define FDT_DEFAULT_ADDR 0x87f00000u

/* Logical CPU modelled by the emulator (hartid 0, RV32IMAF). */
#define FDT_HART_COUNT 1u
#define FDT_HART_BASE 0u

/*
 * Opaque handle to an in-progress FDT build.  It encodes the address of the
 * owning struct fdt_builder, so it must be pointer-width (uintptr_t) -- never
 * a 32-bit type, or the pointer would be truncated on 64-bit hosts.
 *
 * Helper "fdt_" functions treat 0 as "no handle" (an error return).
 */
typedef uintptr_t fdt_handle_t;

/*
 * Build context.  Used by fdt_build_riscvemu() which serialises into
 * g_state.main_memory, but kept generic so callers can point a builder at any
 * writable buffer.
 */
struct fdt_builder
{
    /* Output buffer (the whole tree is written here, header first). */
    uint8_t *buf;
    size_t bufsize;

    /* --- Private: mutable build state (managed by fdt.c). --- */
    uint32_t struct_off;   /* current write offset in the structure block */
    uint32_t strings_off;  /* current write offset in the strings block */
    uint32_t rsvmap_count; /* 64-bit reserve entries, excluding terminator */
    int depth;             /* nest depth of pushed named nodes */
    int err;               /* sticky error latch: any failed op poisons us */
};

/* ------------------------------------------------------------------ */
/* Builder primitives (write-only encoding of the FDT binary format). */
/* ------------------------------------------------------------------ */

/*
 * Prepare `b` to serialise a fresh FDT into `buf`/`bufsize`.  `buf` must point
 * at writable storage of at least `bufsize` bytes; the encoder reserves an
 * internal structure window and checks the overall bound at fdt_serialize().
 * Returns 0 on failure (e.g. null buffer), otherwise a builder handle the
 * caller can use to add nodes via fdt_node_push / fdt_node_pop and finally
 * fdt_serialize.
 */
fdt_handle_t fdt_builder_init(struct fdt_builder *b);

/*
 * Enter a child node, and leave it again.  Node names must be NUL-terminated
 * strings and unit addresses should follow the DT spec as `<node>@<addr>`.
 * Handles may nest arbitrarily; the builder tracks the depth for you.
 *
 * NOTE: the root "/" node is opened implicitly by fdt_builder_init (as an
 * empty-name FDT_BEGIN_NODE), so callers only push/pop the named *children* of
 * the root.
 *
 * Pushes return the (updated) handle on success and 0 on a sticky error (an
 * earlier failed allocation/guard).  Popping returns 0 when there is nothing
 * to pop, otherwise the handle.
 */
fdt_handle_t fdt_node_push(fdt_handle_t h, const char *name);
fdt_handle_t fdt_node_pop(fdt_handle_t h);

/*
 * Add a string property.  The string is NUL-terminated for you; the property
 * value stored in the FDT includes that trailing NUL.
 */
fdt_handle_t fdt_prop_str(fdt_handle_t h, const char *name, const char *str);

/*
 * Add an empty flag property (a property with an empty value), e.g.
 * `u-boot,dm-pre-reloc`.  Most often used for boolean markers.
 */
fdt_handle_t fdt_prop_empty(fdt_handle_t h, const char *name);

/*
 * Add an integer property containing up to four 32-bit cells.  Each cell is
 * encoded big-endian, matching the DT binary format.
 */
fdt_handle_t fdt_prop_word(fdt_handle_t h, const char *name, uint32_t v0,
                           uint32_t v1, uint32_t v2, uint32_t v3);

/* Convenience wrappers over fdt_prop_word for the common 1/2/3-cell cases. */
fdt_handle_t fdt_prop_cells1(fdt_handle_t h, const char *name, uint32_t v0);
fdt_handle_t fdt_prop_cells2(fdt_handle_t h, const char *name, uint32_t v0,
                             uint32_t v1);
fdt_handle_t fdt_prop_cells3(fdt_handle_t h, const char *name, uint32_t v0,
                             uint32_t v1, uint32_t v2);

/*
 * Add a raw property from a caller-supplied byte array (invariant-length
 * values such as interrupt maps).  `len` is the byte length in `data`; all the
 * bytes are copied verbatim (big-endian 32-bit groups, if any, are the
 * caller's responsibility).
 */
fdt_handle_t fdt_prop_bytes(fdt_handle_t h, const char *name,
                            const void *data, size_t len);

/*
 * Assign the property that carries the node's unit-interrupt parent.  The
 * caller passes the phandle-cell value of the interrupt controller (or 0 to
 * clear it).  This is a 32-bit cell, big-endian, matching `phandle`.
 */
fdt_handle_t fdt_prop_interrupt_parent(fdt_handle_t h, uint32_t phandle);

/*
 * Serialise the tree described by `h` into the builder's buffer.  On success
 * returns the total size in bytes of the FDT (including the header), or 0 on
 * failure.  After a successful call the builder is consumed (the handle becomes
 * invalid).  The caller is responsible for making the buffer accessible to the
 * guest (e.g. it lives inside g_state.main_memory).
 */
size_t fdt_serialize(fdt_handle_t h);

/*
 * Mark the node we are currently inside of as `status = "okay"`.  OpenSBI's
 * generic platform skips nodes that are disabled, so anything intended to be
 * used must end up enabled.  Nodes are enabled by default in the DT spec; this
 * helper exists for completeness/symmetry with the rest of the builder.
 */
fdt_handle_t fdt_mark_enabled(fdt_handle_t h);

/* ------------------------------------------------------------------ */
/* Machine description.                                               */
/* ------------------------------------------------------------------ */

/*
 * Build the complete FDT for the machine this emulator models and write it
 * into `g_state.main_memory` at `addr` (which must be a RAM location not
 * overlapped by the loaded firmware).  `addr` should be 8-byte aligned.
 *
 * Returns the number of bytes written, or 0 on error.  On success the FDT is
 * ready to hand to firmware via a1.
 */
size_t fdt_build_riscvemu(uint32_t addr);

#endif /* CONFIG_ENABLE_FDT */

#endif /* FDT_H */
