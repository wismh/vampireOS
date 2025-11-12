#include "idt.h"
#include "vga.h"

__attribute__((section(".text.kmain"), noreturn))
void kmain(void)
{
    vga_clear();
    vga_write_at(0, 0, "Vampire OS");
    vga_write_at(2, 0, "kbd>");
    idt_init();
    __asm__ volatile ("sti");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
