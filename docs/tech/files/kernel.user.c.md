# `kernel/user.c`

Source: [`kernel/user.c`](../../../kernel/user.c)

Module: [Tasks](../modules/tasks.md)

int 0x30 switch; user_run; GDT/TSS glue. Console `write` uses `vga_putc` + `kbd_console_sync`.
