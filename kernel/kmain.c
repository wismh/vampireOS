typedef unsigned short u16;

static void vga_write(const char *msg)
{
    volatile u16 *vga = (volatile u16 *)0xB8000;
    int i;

    for (i = 0; i < 80 * 25; i++) {
        vga[i] = 0x0F20;
    }

    for (i = 0; msg[i] != '\0'; i++) {
        vga[i] = (u16)msg[i] | 0x0F00;
    }
}

__attribute__((section(".text.kmain"), noreturn))
void kmain(void)
{
    vga_write("Vampire OS");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
