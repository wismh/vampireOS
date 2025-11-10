#include "idt.h"
#include "vga.h"

__attribute__((section(".text.kmain"), noreturn))
void kmain(void)
{
    vga_clear();
    vga_write_at(0, 0, "Vampire OS");
    idt_init();
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
