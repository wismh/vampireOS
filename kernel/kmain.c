#include "ahci.h"
#include "e820.h"
#include "fb.h"
#include "fs.h"
#include "heap.h"
#include "idt.h"
#include "kbd.h"
#include "pmm.h"
#include "sched.h"
#include "user.h"
#include "vga.h"
#include "virtio.h"
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
    row = virtio_init(row);
    row = ahci_init(row);
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
