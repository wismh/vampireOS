# `kernel/vga.c`

Source: [`kernel/vga.c`](../../../kernel/vga.c)

Module: [Console](../modules/console.md)

80×25 through HHDM plus a RAM copy for the LFB blit (VBE scanout is not 0xB8000). `vga_putc` also writes COM1 and does not draw the overlay.
