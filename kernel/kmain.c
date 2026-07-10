#include "ahci.h"
#include "ata.h"
#include "bio.h"
#include "e820.h"
#include "fb.h"
#include "fs.h"
#include "heap.h"
#include "idt.h"
#include "kbd.h"
#include "pmm.h"
#include "sched.h"
#include "serial.h"
#include "user.h"
#include "vga.h"
#include "virtio.h"
#include "vmm.h"

static int boot_row;

__attribute__((noreturn))
static void kmain_cont(void)
{
    int row = boot_row;
    char pmsg[16];
    unsigned v;
    unsigned n;
    unsigned d;

    idt_init();
    row = vmm_drop_identity(row);
    row = vmm_print(row);
    kheap_init();
    row = kheap_print(row);
    row = virtio_init(row);
    row = ahci_init(row);
    row = ata_init(row);
    if (bio_init() != 0) {
        vga_write_at(row, 0, "part fail");
        fb_draw_text(8, 88, "part fail");
    } else {
        v = bio_part_lba();
        vga_write_at(row, 0, "part");
        vga_write_dec_at(row, 5, v);
        pmsg[0] = 'p';
        pmsg[1] = 'a';
        pmsg[2] = 'r';
        pmsg[3] = 't';
        pmsg[4] = ' ';
        n = 0;
        d = 1;
        while (d <= v / 10u) {
            d *= 10u;
        }
        while (d > 0) {
            pmsg[5 + n] = (char)('0' + (v / d) % 10u);
            n++;
            d /= 10u;
        }
        pmsg[5 + n] = '\0';
        fb_draw_text(8, 88, pmsg);
    }
    row++;
    row = fs_init(row);
    row = user_init(row);
    vga_set_cursor(row, 0);
    kbd_console_init();
    if (user_ready()) {
        /* Boot init; it fork/execs sh and restarts it. Kernel kbd> is fallback. */
        if (user_run("init", 0) != 0) {
            kbd_prompt();
        } else {
            /* Do not Ctrl+C the reaper; kernel `run` will note a new fg. */
            sched_clear_fg();
        }
        user_enter();
    }
    kbd_prompt();
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
    serial_init();
    vga_write_at(0, 0, "Vampire OS");
    vga_write_hex64_at(0, 12, (uint64_t)(uintptr_t)kmain);
    row = e820_print(map);
    pmm_init(map);
    row = pmm_print(row);
    vmm_map_usable(map);
    vmm_hhdm_init();
    fb_init();
    boot_row = row;
    vmm_switch_stack(kmain_cont);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
