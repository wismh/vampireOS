#include "e820.h"
#include "idt.h"
#include "pmm.h"
#include "vga.h"

__attribute__((section(".text.kmain"), noreturn))
void kmain(const struct e820_map *map)
{
    int row;

    vga_clear();
    vga_write_at(0, 0, "Vampire OS");
    row = e820_print(map);
    pmm_init(map);
    row = pmm_print(row);
    vga_write_at(row, 0, "kbd>");
    vga_set_cursor(row + 1, 0);
    idt_init();
    __asm__ volatile ("sti");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
