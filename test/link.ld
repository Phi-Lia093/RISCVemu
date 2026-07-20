OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x0;

    .text : {
        KEEP(*(.text._start))
        *(.text)
        . = ALIGN(16);
        _stack_start = .;
        . += 8192;
        _stack_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.riscv.attributes)
    }
    PROVIDE(_stack_top = _stack_end);
    }
