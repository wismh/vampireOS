#include "ata.h"
#include "e820.h"
#include "fs.h"
#include "heap.h"
#include "idt.h"
#include "kbd.h"
#include "pmm.h"
#include "user.h"
#include "vga.h"
#include "vmm.h"

static int boot_row;

__attribute__((noreturn))
static void kmain_cont(void)
{
    int row = boot_row;

    idt_init();
    row = vmm_drop_identity(row);
    row = vmm_print(row);
    kheap_init();
    row = kheap_print(row);
    {
        uint64_t phys;
        void *rd;
        /* Must match boot/const.inc INITRD_LBA / INITRD_SECTORS. */
        unsigned bytes = 8u * 512u;
        uint32_t lba = 17u + 48u;

        phys = pmm_alloc_above(0x200000ull);
        if (phys == 0) {
            phys = pmm_alloc();
        }
        if (phys == 0) {
            vga_write_at(row, 0, "ata fail");
            fs_init(0, 0);
        } else {
            rd = (void *)(uintptr_t)phys_to_virt(phys);
            if (ata_read(lba, 8u, rd) != 0) {
                vga_write_at(row, 0, "ata fail");
                fs_init(0, 0);
            } else {
                vga_write_at(row, 0, "ata ");
                vga_write_dec_at(row, 4, bytes);
                fs_init(rd, bytes);
            }
        }
        row++;
    }
    row = user_init(row);
    vga_set_cursor(row, 0);
    kbd_console_init();
    if (user_ready()) {
        user_enter();
    }
    __asm__ volatile ("sti");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

__attribute__((section(".text.kmain"), noreturn))
void kmain(const struct e820_map *map)
{
    int row;

    vga_clear();
    vga_write_at(0, 0, "Vampire OS");
    vga_write_hex64_at(0, 12, (uint64_t)(uintptr_t)kmain);
    row = e820_print(map);
    pmm_init(map);
    row = pmm_print(row);
    vmm_map_usable(map);
    vmm_hhdm_init();
    boot_row = row;
    vmm_switch_stack(kmain_cont);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
