#include "vmm.h"
#include "pmm.h"
#include "vga.h"

#include <stdint.h>

/* Must match boot/const.inc and the 2 MiB page in stage 2. */
#define PML4_PHYS 0x1000ull
#define PD_PHYS 0x3000ull
#define PAGE_4K 0x1000ull
#define PAGE_2M 0x200000ull
#define PD_ENTRIES 512ull
#define PT_ENTRIES 512ull
#define PDE_PRESENT 1ull
#define PDE_WRITE 2ull
#define PDE_LARGE 0x80ull
#define PDE_HUGE (PDE_PRESENT | PDE_WRITE | PDE_LARGE)
#define PTE_FLAGS (PDE_PRESENT | PDE_WRITE)
#define ADDR_MASK ~0xFFFull
#define PROBE_MARK 0x56414D50u
#define HHDM_PML4_INDEX 256ull

static uint64_t first_4k_mapped;
static int hhdm_ready;

static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static uint64_t align_down(uint64_t value, uint64_t align)
{
    return value & ~(align - 1);
}

static volatile uint64_t *kmap(uint64_t phys)
{
    return (volatile uint64_t *)(uintptr_t)phys_to_virt(phys);
}

static int vmm_map_4k(uint64_t phys)
{
    volatile uint64_t *pd = kmap(PD_PHYS);
    uint64_t pd_idx = phys >> 21;
    uint64_t pt_idx = (phys >> 12) & (PT_ENTRIES - 1);
    uint64_t pde;
    volatile uint64_t *pt;
    uint64_t i;

    if ((phys & (PAGE_4K - 1)) != 0 || pd_idx >= PD_ENTRIES) {
        return -1;
    }

    pde = pd[pd_idx];
    if ((pde & PDE_LARGE) != 0) {
        return -1;
    }

    if ((pde & PDE_PRESENT) == 0) {
        uint64_t pt_phys = pmm_alloc();

        if (pt_phys == 0 || pt_phys >= PAGE_2M) {
            if (pt_phys != 0) {
                pmm_free(pt_phys);
            }
            return -1;
        }
        pt = kmap(pt_phys);
        for (i = 0; i < PT_ENTRIES; i++) {
            pt[i] = 0;
        }
        pde = pt_phys | PTE_FLAGS;
        pd[pd_idx] = pde;
    }

    pt = kmap(pde & ADDR_MASK);
    pt[pt_idx] = phys | PTE_FLAGS;
    if (first_4k_mapped == 0) {
        first_4k_mapped = phys;
    }
    return 0;
}

static void vmm_map_2m(const struct e820_map *map, uint32_t n)
{
    volatile uint64_t *pd = kmap(PD_PHYS);
    uint32_t i;

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
}

static void vmm_map_tail_4k(const struct e820_map *map, uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n; i++) {
        const struct e820_entry *e = &map->entries[i];
        uint64_t start;
        uint64_t end;
        uint64_t huge_lo;
        uint64_t huge_hi;
        uint64_t addr;

        if (e->type != E820_TYPE_USABLE || e->length == 0) {
            continue;
        }
        end = e->base + e->length;
        if (end < e->base) {
            continue;
        }
        huge_lo = align_up(e->base, PAGE_2M);
        huge_hi = align_down(end, PAGE_2M);
        start = align_up(e->base, PAGE_4K);
        end = align_down(end, PAGE_4K);
        for (addr = start; addr < end; addr += PAGE_4K) {
            if (addr < PAGE_2M) {
                continue;
            }
            if (huge_lo < huge_hi && addr >= huge_lo && addr < huge_hi) {
                continue;
            }
            if (vmm_map_4k(addr) != 0) {
                return;
            }
        }
    }
}

void vmm_map_usable(const struct e820_map *map)
{
    uint32_t n = 0;
    uint64_t pml4 = PML4_PHYS;

    first_4k_mapped = 0;

    if (map != 0 && map->entry_size == E820_ENTRY_SIZE) {
        n = map->count;
        if (n > E820_MAX) {
            n = E820_MAX;
        }
    }

    vmm_map_2m(map, n);
    vmm_map_tail_4k(map, n);
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4) : "memory");
}

void vmm_hhdm_init(void)
{
    uint64_t pdpt_phys;
    volatile uint64_t *pdpt;
    volatile uint64_t *pml4;
    uint64_t i;
    uint64_t cr3 = PML4_PHYS;

    hhdm_ready = 0;
    pdpt_phys = pmm_alloc();
    if (pdpt_phys == 0 || pdpt_phys >= PAGE_2M) {
        if (pdpt_phys != 0) {
            pmm_free(pdpt_phys);
        }
        return;
    }

    pdpt = kmap(pdpt_phys);
    for (i = 0; i < PD_ENTRIES; i++) {
        pdpt[i] = 0;
    }
    pdpt[0] = PD_PHYS | PTE_FLAGS;
    pml4 = kmap(PML4_PHYS);
    pml4[HHDM_PML4_INDEX] = pdpt_phys | PTE_FLAGS;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
    hhdm_ready = 1;
}

uint64_t phys_to_virt(uint64_t phys)
{
    if (!hhdm_ready) {
        return phys;
    }
    return HHDM_BASE + phys;
}

uint64_t virt_to_phys(uint64_t virt)
{
    if (hhdm_ready && virt >= HHDM_BASE) {
        return virt - HHDM_BASE;
    }
    return virt;
}

static int vmm_probe(int row, const char *ok_msg, const char *fail_msg, uint64_t phys)
{
    volatile uint32_t *p;

    if (phys == 0) {
        vga_write_at(row, 0, fail_msg);
        return row + 1;
    }

    p = (volatile uint32_t *)(uintptr_t)phys_to_virt(phys);
    *p = PROBE_MARK;
    if (*p == PROBE_MARK) {
        vga_write_at(row, 0, ok_msg);
        vga_write_hex64_at(row, 7, phys);
    } else {
        vga_write_at(row, 0, fail_msg);
        vga_write_hex64_at(row, 9, phys);
    }
    pmm_free(phys);
    return row + 1;
}

int vmm_print(int row)
{
    uint64_t phys;

    if (row >= VGA_HEIGHT - 3) {
        return row;
    }

    phys = pmm_alloc_above(PAGE_2M);
    row = vmm_probe(row, "map ok ", "map fail", phys);

    if (row >= VGA_HEIGHT - 2) {
        return row;
    }

    phys = 0;
    if (first_4k_mapped != 0) {
        phys = pmm_alloc_above(first_4k_mapped);
    }
    return vmm_probe(row, "pt ok  ", "pt fail", phys);
}

__attribute__((noreturn))
void vmm_switch_stack(void (*cont)(void))
{
    uint64_t phys;
    uint64_t top;
    uint8_t *page;
    uint64_t i;

    phys = pmm_alloc_above(PAGE_2M);
    if (phys == 0) {
        phys = pmm_alloc();
    }
    if (phys == 0 || cont == 0) {
        if (cont != 0) {
            cont();
        }
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    page = (uint8_t *)(uintptr_t)phys_to_virt(phys);
    for (i = 0; i < PAGE_4K; i++) {
        page[i] = 0;
    }
    /* SysV: at function entry rsp % 16 == 8 (as after a call). */
    top = phys_to_virt(phys) + PAGE_4K - 8;
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "xor %%rbp, %%rbp\n"
        "jmp *%1\n"
        :
        : "r"(top), "r"(cont)
        : "memory"
    );
    __builtin_unreachable();
}
