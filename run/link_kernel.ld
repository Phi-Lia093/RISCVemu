/* SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/*
 * Linker script for the fake next-stage kernel.
 *
 * OpenSBI fw_jump (built with FW_JUMP_OFFSET=0x400000 on this RV32 platform)
 * jumps to the next stage at firmware_base + offset = 0x80400000.  The kernel
 * must be present (and self-contained) at exactly that address.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    /* Next-stage hand-off point used by OpenSBI fw_jump on this platform. */
    KERNEL (rx) : ORIGIN = 0x80400000, LENGTH = 4M
}

SECTIONS
{
    . =0x80400000;

    .text : {
        . = ALIGN(16);
        KEEP(*(.text._start))
        *(.text)
        . = ALIGN(16);
        _stack_start = .;
        . += 8192;
        _stack_end = .;
    } > KERNEL

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.riscv.attributes)
    }
    PROVIDE(_stack_top = _stack_end);
}
