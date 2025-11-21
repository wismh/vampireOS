ENTRY(kmain)

KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_VMA + 0x100000;

    .text : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text.kmain)
        *(.text*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata*)
    }

    .data : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data*)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VMA) {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.eh_frame*)
        *(.comment)
        *(.note*)
    }
}
