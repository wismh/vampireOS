#include "user.h"
#include "elf.h"
#include "fs.h"
#include "pmm.h"
#include "sched.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

/* Must match boot/stage2.asm kernel selectors and idt.c. */
#define KERNEL_CS 0x10
#define KERNEL_DS 0x18
#define USER_CS 0x2B
#define USER_DS 0x23
#define TSS_SEL 0x30
#define PAGE_4K 0x1000ull
/* Code at BASE, stack page at BASE+0x1000, stack top BASE+0x2000.
 * Distinct BASEs while CR3 is shared; same address waits on week 3. */
#define USER_SLOT 0x2000ull
#define USER_BASE 0x400000ull
#define USER_BASE_A (USER_BASE + 0ull * USER_SLOT)
#define USER_BASE_B (USER_BASE + 1ull * USER_SLOT)
#define USER_BASE_C (USER_BASE + 2ull * USER_SLOT)
#define USER_BASE_E (USER_BASE + 3ull * USER_SLOT)
#define USER_LIMIT (USER_BASE + 4ull * USER_SLOT)
#define KERNEL_STACK_MIN 0x200000ull
#define SYS_WRITE 1ull
#define SYS_EXIT 2ull
#define SYS_YIELD 3ull
#define SYS_SLEEP 4ull
#define SYS_WAIT 5ull
#define USER_STR_MAX 80ull

#define GDT_KERNEL_CODE32 0x00CF9A000000FFFFULL
#define GDT_KERNEL_CODE64 0x00209A0000000000ULL
#define GDT_KERNEL_DATA 0x00CF92000000FFFFULL
#define GDT_USER_DATA 0x00CFF2000000FFFFULL
#define GDT_USER_CODE64 0x0020FA0000000000ULL

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
} __attribute__((packed));

struct tss_desc {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t flags1;
    uint8_t flags2;
    uint8_t base_hi;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

static uint64_t gdt[8] __attribute__((aligned(8)));
static struct tss tss __attribute__((aligned(16)));
static int gdt_ready;
static int enter_ready;
static int user_row;
static int run_row;
static int echo_running;
static uint64_t enter_rip;

_Static_assert(sizeof(struct tss) == 104, "long-mode TSS is 104 bytes");
_Static_assert(sizeof(struct tss_desc) == 16, "TSS descriptor is 16 bytes");

static void zero_page(uint64_t phys)
{
    uint8_t *p = (uint8_t *)(uintptr_t)phys_to_virt(phys);
    uint64_t i;

    for (i = 0; i < PAGE_4K; i++) {
        p[i] = 0;
    }
}

static uint64_t alloc_page(void)
{
    uint64_t phys;

    phys = pmm_alloc_above(KERNEL_STACK_MIN);
    if (phys == 0) {
        phys = pmm_alloc();
    }
    if (phys != 0) {
        zero_page(phys);
    }
    return phys;
}

static uint64_t user_stack_top(uint64_t base)
{
    return base + 2ull * PAGE_4K;
}

static void gdt_set_tss(void)
{
    uint64_t base = (uint64_t)(uintptr_t)&tss;
    uint32_t limit = (uint32_t)sizeof(tss) - 1u;
    struct tss_desc *d = (struct tss_desc *)&gdt[6];

    d->limit_low = (uint16_t)(limit & 0xFFFF);
    d->base_low = (uint16_t)(base & 0xFFFF);
    d->base_mid = (uint8_t)((base >> 16) & 0xFF);
    d->flags1 = 0x89;
    d->flags2 = (uint8_t)((limit >> 16) & 0x0F);
    d->base_hi = (uint8_t)((base >> 24) & 0xFF);
    d->base_upper = (uint32_t)(base >> 32);
    d->reserved = 0;
}

uint64_t gdt_base(void)
{
    return (uint64_t)(uintptr_t)gdt;
}

int gdt_init(void)
{
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    uint8_t *p;
    uint64_t i;

    if (gdt_ready) {
        return 0;
    }

    gdt[0] = 0;
    gdt[1] = GDT_KERNEL_CODE32;
    gdt[2] = GDT_KERNEL_CODE64;
    gdt[3] = GDT_KERNEL_DATA;
    gdt[4] = GDT_USER_DATA;
    gdt[5] = GDT_USER_CODE64;
    gdt[6] = 0;
    gdt[7] = 0;

    p = (uint8_t *)&tss;
    for (i = 0; i < sizeof(tss); i++) {
        p[i] = 0;
    }
    tss.iopb = (uint16_t)sizeof(tss);
    gdt_set_tss();

    gdtr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtr.base = (uint64_t)(uintptr_t)gdt;
    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");
    __asm__ volatile (
        "pushq %0\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov %1, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        :
        : "i"(KERNEL_CS), "i"(KERNEL_DS)
        : "rax", "memory"
    );
    gdt_ready = 1;
    return 0;
}

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}

int user_ready(void)
{
    return enter_ready;
}

/* Map code+stack at BASE, load ELF into the code page. */
static int map_load_elf(const void *data, unsigned len, uint64_t base,
                        uint64_t *entry, uint64_t *stack_top)
{
    uint64_t code;
    uint64_t stack;
    uint8_t *dest;

    code = alloc_page();
    stack = alloc_page();
    if (code == 0 || stack == 0) {
        return -1;
    }
    dest = (uint8_t *)(uintptr_t)phys_to_virt(code);
    if (elf_load(data, len, dest, (unsigned)PAGE_4K, base, entry) != 0) {
        return -1;
    }
    if (vmm_map_user(base, code) != 0 ||
        vmm_map_user(base + PAGE_4K, stack) != 0) {
        return -1;
    }
    *stack_top = user_stack_top(base);
    return 0;
}

static int map_load_elf_name(const char *name, uint64_t base, uint64_t *entry,
                             uint64_t *stack_top)
{
    const void *data;
    unsigned len;

    if (fs_lookup(name, &data, &len) != 0) {
        return -1;
    }
    return map_load_elf(data, len, base, entry, stack_top);
}

int user_init(int row)
{
    uint64_t kstack_a;
    uint64_t kstack_b;
    uint64_t kstack_c;
    uint64_t ktop_a;
    uint64_t ktop_b;
    uint64_t ktop_c;
    uint64_t entry_a;
    uint64_t entry_b;
    uint64_t entry_c;
    uint64_t stack_a;
    uint64_t stack_b;
    uint64_t stack_c;
    uint16_t tr = TSS_SEL;

    enter_ready = 0;
    enter_rip = USER_BASE_A;
    user_row = row;
    if (row >= VGA_HEIGHT - 6) {
        return row;
    }

    if (!gdt_ready) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    kstack_a = alloc_page();
    kstack_b = alloc_page();
    kstack_c = alloc_page();
    if (kstack_a == 0 || kstack_b == 0 || kstack_c == 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    ktop_a = phys_to_virt(kstack_a) + PAGE_4K;
    ktop_b = phys_to_virt(kstack_b) + PAGE_4K;
    ktop_c = phys_to_virt(kstack_c) + PAGE_4K;
    tss_set_rsp0(ktop_a);
    __asm__ volatile ("ltr %0" : : "r"(tr) : "memory");

    if (map_load_elf_name("a", USER_BASE_A, &entry_a, &stack_a) != 0 ||
        map_load_elf_name("b", USER_BASE_B, &entry_b, &stack_b) != 0 ||
        map_load_elf_name("c", USER_BASE_C, &entry_c, &stack_c) != 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    sched_init();
    if (sched_add_user(entry_a, stack_a, ktop_a, row + 1) != 0 ||
        sched_add_user(entry_b, stack_b, ktop_b, row + 2) != 0 ||
        sched_add_user(entry_c, stack_c, ktop_c, row + 3) != 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    vga_write_at(row, 0, "ksp");
    vga_write_hex64_at(row, 4, ktop_a);
    vga_write_hex64_at(row, 21, ktop_b);
    vga_write_hex64_at(row, 38, ktop_c);
    vga_write_at(row + 1, 0, "A --");
    vga_write_at(row + 2, 0, "B --");
    vga_write_at(row + 3, 0, "C --");
    vga_write_at(row + 4, 0, "E --");

    enter_rip = entry_a;
    enter_ready = 1;
    run_row = row + 4;
    echo_running = 0;
    return row + 5;
}

static int copy_from_user(char *dst, uint64_t src, uint64_t max)
{
    uint64_t i;
    const volatile uint8_t *p;

    if (max == 0 || src < USER_BASE || src >= USER_LIMIT) {
        return -1;
    }

    p = (const volatile uint8_t *)(uintptr_t)src;
    for (i = 0; i < max; i++) {
        if (src + i >= USER_LIMIT) {
            return -1;
        }
        dst[i] = (char)p[i];
        if (dst[i] == '\0') {
            return 0;
        }
    }
    return -1;
}

int user_run(const char *name)
{
    const void *data;
    unsigned len;
    uint64_t entry;
    uint64_t kstack;
    uint64_t ktop;
    uint64_t stack_top;

    if (!enter_ready || echo_running || name == 0 || *name == '\0') {
        return -1;
    }
    if (fs_lookup(name, &data, &len) != 0) {
        return -1;
    }
    kstack = alloc_page();
    if (kstack == 0) {
        return -1;
    }
    if (map_load_elf(data, len, USER_BASE_E, &entry, &stack_top) != 0) {
        return -1;
    }
    ktop = phys_to_virt(kstack) + PAGE_4K;
    if (sched_add_user(entry, stack_top, ktop, run_row) != 0) {
        return -1;
    }
    echo_running = 1;
    return 0;
}

void user_on_syscall(struct interrupt_frame *frame)
{
    char buf[USER_STR_MAX];
    int row;
    unsigned n;

    if (frame == 0 || (frame->cs & 3ull) != 3ull) {
        vga_write_at(user_row, 0, "user fail");
        return;
    }
    if (frame->rax == SYS_WRITE) {
        if (copy_from_user(buf, frame->rdi, USER_STR_MAX) != 0) {
            vga_write_at(user_row, 0, "user fail");
            return;
        }
        row = sched_row();
        n = sched_note_write();
        if (n <= 8u || (n & 31u) == 0) {
            vga_write_at(row, 0, buf);
            vga_write_at(row, 2, "          ");
            vga_write_dec_at(row, 2, n);
        }
        return;
    }
    if (frame->rax == SYS_EXIT) {
        sched_exit(frame);
        return;
    }
    if (frame->rax == SYS_YIELD) {
        sched_yield(frame);
        return;
    }
    if (frame->rax == SYS_SLEEP) {
        sched_sleep(frame, frame->rdi);
        return;
    }
    if (frame->rax == SYS_WAIT) {
        sched_wait(frame);
        return;
    }
    vga_write_at(user_row, 0, "user fail");
}

__attribute__((noreturn))
void user_enter(void)
{
    uint64_t rip = enter_rip;
    uint64_t rsp = user_stack_top(USER_BASE_A);
    uint64_t rflags = 0x202;

    if (!enter_ready) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    __asm__ volatile (
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "pushq %0\n"
        "pushq %1\n"
        "pushq %2\n"
        "pushq %3\n"
        "pushq %4\n"
        "iretq\n"
        :
        : "i"(USER_DS), "r"(rsp), "r"(rflags), "i"(USER_CS), "r"(rip)
        : "ax", "memory"
    );
    __builtin_unreachable();
}
