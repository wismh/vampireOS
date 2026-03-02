#include "vmm.h"
#include "pmm.h"
#include "user.h"
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
#define PTE_USER 4ull
#define PTE_USER_FLAGS (PTE_FLAGS | PTE_USER)
#define ADDR_MASK ~0xFFFull
#define PROBE_MARK 0x56414D50u
#define HHDM_PML4_INDEX 256ull
#define KERNEL_PML4_INDEX 511ull

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

uint64_t vmm_boot_cr3(void)
{
    return PML4_PHYS;
}

void vmm_set_cr3(uint64_t cr3)
{
    if (cr3 == 0) {
        cr3 = PML4_PHYS;
    }
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/* Fresh PML4 page: copy HHDM/kernel half from boot; low (user) half stays zero.
 * Map privately with vmm_map_user into this CR3. */
uint64_t vmm_clone_pml4(void)
{
    uint64_t new_phys;
    volatile uint64_t *src;
    volatile uint64_t *dst;
    uint64_t i;

    if (!hhdm_ready) {
        return 0;
    }
    new_phys = pmm_alloc();
    if (new_phys == 0) {
        return 0;
    }
    src = kmap(PML4_PHYS);
    dst = kmap(new_phys);
    for (i = 0; i < PD_ENTRIES; i++) {
        if (i >= HHDM_PML4_INDEX) {
            dst[i] = src[i];
        } else {
            dst[i] = 0;
        }
    }
    return new_phys;
}

static void vmm_flush_if_current(uint64_t cr3)
{
    uint64_t cur;

    __asm__ volatile ("mov %%cr3, %0" : "=r"(cur));
    if (cur == cr3) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }
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

static int vmm_ensure_table(volatile uint64_t *table, uint64_t idx, uint64_t flags,
                            volatile uint64_t **out)
{
    uint64_t e = table[idx];
    uint64_t phys;
    uint64_t i;
    volatile uint64_t *next;

    if ((e & PDE_PRESENT) == 0) {
        phys = pmm_alloc();
        if (phys == 0) {
            return -1;
        }
        next = kmap(phys);
        for (i = 0; i < PT_ENTRIES; i++) {
            next[i] = 0;
        }
        table[idx] = phys | flags;
        *out = next;
        return 0;
    }
    if ((e & PDE_LARGE) != 0) {
        return -1;
    }
    if ((flags & PTE_USER) != 0 && (e & PTE_USER) == 0) {
        table[idx] = e | PTE_USER;
    }
    *out = kmap(e & ADDR_MASK);
    return 0;
}

int vmm_map_user(uint64_t cr3, uint64_t virt, uint64_t phys)
{
    volatile uint64_t *pml4;
    volatile uint64_t *pdpt;
    volatile uint64_t *pd;
    volatile uint64_t *pt;
    uint64_t pml4_i = (virt >> 39) & (PD_ENTRIES - 1);
    uint64_t pdpt_i = (virt >> 30) & (PD_ENTRIES - 1);
    uint64_t pd_i = (virt >> 21) & (PD_ENTRIES - 1);
    uint64_t pt_i = (virt >> 12) & (PT_ENTRIES - 1);

    if (cr3 == 0) {
        cr3 = PML4_PHYS;
    }
    if ((virt & (PAGE_4K - 1)) != 0 || (phys & (PAGE_4K - 1)) != 0) {
        return -1;
    }
    if (pml4_i == HHDM_PML4_INDEX || pml4_i == KERNEL_PML4_INDEX) {
        return -1;
    }
    if (!hhdm_ready) {
        return -1;
    }

    pml4 = kmap(cr3);
    if (vmm_ensure_table(pml4, pml4_i, PTE_USER_FLAGS, &pdpt) != 0) {
        return -1;
    }
    if (vmm_ensure_table(pdpt, pdpt_i, PTE_USER_FLAGS, &pd) != 0) {
        return -1;
    }
    if (vmm_ensure_table(pd, pd_i, PTE_USER_FLAGS, &pt) != 0) {
        return -1;
    }
    pt[pt_i] = phys | PTE_USER_FLAGS;
    vmm_flush_if_current(cr3);
    return 0;
}

int vmm_unmap_user(uint64_t cr3, uint64_t virt)
{
    volatile uint64_t *pml4;
    volatile uint64_t *pdpt;
    volatile uint64_t *pd;
    volatile uint64_t *pt;
    uint64_t pml4_i = (virt >> 39) & (PD_ENTRIES - 1);
    uint64_t pdpt_i = (virt >> 30) & (PD_ENTRIES - 1);
    uint64_t pd_i = (virt >> 21) & (PD_ENTRIES - 1);
    uint64_t pt_i = (virt >> 12) & (PT_ENTRIES - 1);
    uint64_t e;
    uint64_t phys;

    if (cr3 == 0) {
        cr3 = PML4_PHYS;
    }
    if ((virt & (PAGE_4K - 1)) != 0) {
        return -1;
    }
    if (pml4_i == HHDM_PML4_INDEX || pml4_i == KERNEL_PML4_INDEX) {
        return -1;
    }
    if (!hhdm_ready) {
        return -1;
    }

    pml4 = kmap(cr3);
    e = pml4[pml4_i];
    if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pdpt = kmap(e & ADDR_MASK);
    e = pdpt[pdpt_i];
    if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pd = kmap(e & ADDR_MASK);
    e = pd[pd_i];
    if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pt = kmap(e & ADDR_MASK);
    e = pt[pt_i];
    if ((e & PDE_PRESENT) == 0) {
        return -1;
    }
    phys = e & ADDR_MASK;
    pt[pt_i] = 0;
    vmm_flush_if_current(cr3);
    pmm_free(phys);
    return 0;
}

static int vmm_table_clear(volatile uint64_t *table, uint64_t n)
{
    uint64_t i;

    for (i = 0; i < n; i++) {
        if (table[i] != 0) {
            return 0;
        }
    }
    return 1;
}

void vmm_teardown_user(uint64_t cr3)
{
    volatile uint64_t *pml4;
    volatile uint64_t *pdpt;
    volatile uint64_t *pd;
    volatile uint64_t *pt;
    uint64_t pml4_i;
    uint64_t pdpt_i;
    uint64_t pd_i;
    uint64_t pt_i;
    uint64_t e;
    uint64_t pdpt_phys;
    uint64_t pd_phys;
    uint64_t pt_phys;
    uint64_t leaf;

    if (cr3 == 0 || cr3 == PML4_PHYS || !hhdm_ready) {
        return;
    }

    pml4 = kmap(cr3);
    for (pml4_i = 0; pml4_i < HHDM_PML4_INDEX; pml4_i++) {
        e = pml4[pml4_i];
        if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
            continue;
        }
        pdpt_phys = e & ADDR_MASK;
        pdpt = kmap(pdpt_phys);
        for (pdpt_i = 0; pdpt_i < PD_ENTRIES; pdpt_i++) {
            e = pdpt[pdpt_i];
            if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
                continue;
            }
            pd_phys = e & ADDR_MASK;
            pd = kmap(pd_phys);
            for (pd_i = 0; pd_i < PD_ENTRIES; pd_i++) {
                e = pd[pd_i];
                if ((e & PDE_PRESENT) == 0 || (e & PDE_LARGE) != 0) {
                    continue;
                }
                pt_phys = e & ADDR_MASK;
                pt = kmap(pt_phys);
                for (pt_i = 0; pt_i < PT_ENTRIES; pt_i++) {
                    e = pt[pt_i];
                    if ((e & PDE_PRESENT) != 0 && (e & PTE_USER) != 0) {
                        leaf = e & ADDR_MASK;
                        pt[pt_i] = 0;
                        pmm_free(leaf);
                    }
                }
                if (vmm_table_clear(pt, PT_ENTRIES)) {
                    pd[pd_i] = 0;
                    pmm_free(pt_phys);
                }
            }
            if (vmm_table_clear(pd, PD_ENTRIES)) {
                pdpt[pdpt_i] = 0;
                pmm_free(pd_phys);
            }
        }
        if (vmm_table_clear(pdpt, PD_ENTRIES)) {
            pml4[pml4_i] = 0;
            pmm_free(pdpt_phys);
        }
    }
    vmm_flush_if_current(cr3);
}

static void copy_page(uint64_t dst_phys, uint64_t src_phys)
{
    volatile uint8_t *dst;
    const volatile uint8_t *src;
    uint64_t i;

    dst = (volatile uint8_t *)(uintptr_t)phys_to_virt(dst_phys);
    src = (const volatile uint8_t *)(uintptr_t)phys_to_virt(src_phys);
    for (i = 0; i < PAGE_4K; i++) {
        dst[i] = src[i];
    }
}

int vmm_copy_user(uint64_t dst_cr3, uint64_t src_cr3)
{
    volatile uint64_t *pml4;
    volatile uint64_t *pdpt;
    volatile uint64_t *pd;
    volatile uint64_t *pt;
    uint64_t pml4_i;
    uint64_t pdpt_i;
    uint64_t pd_i;
    uint64_t pt_i;
    uint64_t e;
    uint64_t leaf;
    uint64_t neu;
    uint64_t va;

    if (dst_cr3 == 0 || src_cr3 == 0 || dst_cr3 == src_cr3 ||
        dst_cr3 == PML4_PHYS || src_cr3 == PML4_PHYS || !hhdm_ready) {
        return -1;
    }

    pml4 = kmap(src_cr3);
    for (pml4_i = 0; pml4_i < HHDM_PML4_INDEX; pml4_i++) {
        e = pml4[pml4_i];
        if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 || (e & PDE_LARGE) != 0) {
            continue;
        }
        pdpt = kmap(e & ADDR_MASK);
        for (pdpt_i = 0; pdpt_i < PD_ENTRIES; pdpt_i++) {
            e = pdpt[pdpt_i];
            if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 ||
                (e & PDE_LARGE) != 0) {
                continue;
            }
            pd = kmap(e & ADDR_MASK);
            for (pd_i = 0; pd_i < PD_ENTRIES; pd_i++) {
                e = pd[pd_i];
                if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 ||
                    (e & PDE_LARGE) != 0) {
                    continue;
                }
                pt = kmap(e & ADDR_MASK);
                for (pt_i = 0; pt_i < PT_ENTRIES; pt_i++) {
                    e = pt[pt_i];
                    if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0) {
                        continue;
                    }
                    leaf = e & ADDR_MASK;
                    va = (pml4_i << 39) | (pdpt_i << 30) | (pd_i << 21) |
                         (pt_i << 12);
                    neu = pmm_alloc_above(PAGE_2M);
                    if (neu == 0) {
                        neu = pmm_alloc();
                    }
                    if (neu == 0) {
                        return -1;
                    }
                    copy_page(neu, leaf);
                    if (vmm_map_user(dst_cr3, va, neu) != 0) {
                        pmm_free(neu);
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

int vmm_translate_user(uint64_t cr3, uint64_t virt, uint64_t *phys_out)
{
    volatile uint64_t *pml4;
    volatile uint64_t *pdpt;
    volatile uint64_t *pd;
    volatile uint64_t *pt;
    uint64_t pml4_i = (virt >> 39) & (PD_ENTRIES - 1);
    uint64_t pdpt_i = (virt >> 30) & (PD_ENTRIES - 1);
    uint64_t pd_i = (virt >> 21) & (PD_ENTRIES - 1);
    uint64_t pt_i = (virt >> 12) & (PT_ENTRIES - 1);
    uint64_t e;

    if (phys_out == 0) {
        return -1;
    }
    if (cr3 == 0) {
        cr3 = PML4_PHYS;
    }
    /* Kernel/HHDM halves are never user-accessible. */
    if (pml4_i == HHDM_PML4_INDEX || pml4_i == KERNEL_PML4_INDEX) {
        return -1;
    }
    if (!hhdm_ready) {
        return -1;
    }

    pml4 = kmap(cr3);
    e = pml4[pml4_i];
    if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pdpt = kmap(e & ADDR_MASK);
    e = pdpt[pdpt_i];
    if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pd = kmap(e & ADDR_MASK);
    e = pd[pd_i];
    if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0 || (e & PDE_LARGE) != 0) {
        return -1;
    }
    pt = kmap(e & ADDR_MASK);
    e = pt[pt_i];
    if ((e & PDE_PRESENT) == 0 || (e & PTE_USER) == 0) {
        return -1;
    }
    *phys_out = (e & ADDR_MASK) | (virt & (PAGE_4K - 1));
    return 0;
}

int vmm_drop_identity(int row)
{
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    volatile uint64_t *pml4;
    uint64_t cr3 = PML4_PHYS;

    if (row >= VGA_HEIGHT - 2) {
        return row;
    }
    if (!hhdm_ready || gdt_init() != 0) {
        vga_write_at(row, 0, "id keep");
        return row + 1;
    }

    pml4 = kmap(PML4_PHYS);
    pml4[0] = 0;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");

    __asm__ volatile ("sgdt %0" : "=m"(gdtr));
    if (pml4[0] == 0 && gdtr.base == gdt_base()) {
        vga_write_at(row, 0, "id off ");
        vga_write_hex64_at(row, 7, gdtr.base);
    } else {
        vga_write_at(row, 0, "id fail");
    }
    return row + 1;
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
