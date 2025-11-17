#include "vmm.h"
#include "pmm.h"
#include "vga.h"

#include <stdint.h>

/* Must match boot/const.inc and the 2 MiB page in stage 2. */
#define PML4_PHYS 0x1000ull
#define PD_PHYS 0x3000ull
#define PAGE_2M 0x200000ull
#define PD_ENTRIES 512ull
#define PDE_PRESENT 1ull
#define PDE_WRITE 2ull
#define PDE_LARGE 0x80ull
#define PDE_HUGE (PDE_PRESENT | PDE_WRITE | PDE_LARGE)
#define PROBE_MARK 0x56414D50u

static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static uint64_t align_down(uint64_t value, uint64_t align)
{
    return value & ~(align - 1);
}

void vmm_map_usable(const struct e820_map *map)
{
    volatile uint64_t *pd = (volatile uint64_t *)(uintptr_t)PD_PHYS;
    uint32_t i;
    uint32_t n = 0;
    uint64_t pml4 = PML4_PHYS;

    if (map != 0 && map->entry_size == E820_ENTRY_SIZE) {
        n = map->count;
        if (n > E820_MAX) {
            n = E820_MAX;
        }
    }

    for (i = 0; i < n; i++) {
        const struct e820_entry *e = &map->entries[i];
        uint64_t start;
        uint64_t end;

        if (e->type != E820_TYPE_USABLE || e->length == 0) {
            continue;
        }
        end = e->base + e->length;
        if (end < e->base) {
            continue;
        }
        start = align_up(e->base, PAGE_2M);
        end = align_down(end, PAGE_2M);
        while (start < end) {
            uint64_t idx = start >> 21;

            if (idx >= PD_ENTRIES) {
                break;
            }
            if (idx != 0) {
                pd[idx] = start | PDE_HUGE;
            }
            start += PAGE_2M;
        }
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4) : "memory");
}

int vmm_print(int row)
{
    uint64_t phys;
    volatile uint32_t *p;

    if (row >= VGA_HEIGHT - 2) {
        return row;
    }

    phys = pmm_alloc_above(PAGE_2M);
    if (phys == 0) {
        vga_write_at(row, 0, "map fail");
        return row + 1;
    }

    p = (volatile uint32_t *)(uintptr_t)phys;
    *p = PROBE_MARK;
    if (*p == PROBE_MARK) {
        vga_write_at(row, 0, "map ok ");
        vga_write_hex64_at(row, 7, phys);
    } else {
        vga_write_at(row, 0, "map fail");
        vga_write_hex64_at(row, 9, phys);
    }
    pmm_free(phys);
    return row + 1;
}
