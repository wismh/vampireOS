ENTRY(_start)

PHDRS
{
    load PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.bss*)
        *(COMMON)
    } :load

    /DISCARD/ : {
        *(.eh_frame*)
        *(.comment)
        *(.note*)
        *(.interp)
        *(.dyn*)
        *(.gnu*)
        *(.got*)
        *(.plt*)
    }
}
