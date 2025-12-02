#include "user.h"
#include "pmm.h"
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
#define USER_CODE 0x400000ull
#define USER_STACK 0x401000ull
#define USER_STACK_TOP 0x402000ull
#define KERNEL_STACK_MIN 0x200000ull
#define SYS_WRITE 1ull
#define SYS_EXIT 2ull
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

int user_ready(void)
{
    return enter_ready;
}

int user_init(int row)
{
    uint64_t kstack;
    uint64_t code;
    uint64_t stack;
    uint8_t *p;
    uint16_t tr = TSS_SEL;

    enter_ready = 0;
    user_row = row;
    if (row >= VGA_HEIGHT - 2) {
        return row;
    }
    vga_write_at(row, 0, "user --");

    if (!gdt_ready) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    kstack = alloc_page();
    code = alloc_page();
    stack = alloc_page();
    if (kstack == 0 || code == 0 || stack == 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    tss.rsp0 = phys_to_virt(kstack) + PAGE_4K;
    __asm__ volatile ("ltr %0" : : "r"(tr) : "memory");

    if (vmm_map_user(USER_CODE, code) != 0 ||
        vmm_map_user(USER_STACK, stack) != 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    /* mov eax,1 ; mov edi,str ; int 0x30 ; mov eax,2 ; int 0x30 ; ud2 ; "hi" */
    p = (uint8_t *)(uintptr_t)phys_to_virt(code);
    p[0] = 0xB8;
    p[1] = 0x01;
    p[2] = 0x00;
    p[3] = 0x00;
    p[4] = 0x00;
    p[5] = 0xBF;
    p[6] = 0x15;
    p[7] = 0x00;
    p[8] = 0x40;
    p[9] = 0x00;
    p[10] = 0xCD;
    p[11] = 0x30;
    p[12] = 0xB8;
    p[13] = 0x02;
    p[14] = 0x00;
    p[15] = 0x00;
    p[16] = 0x00;
    p[17] = 0xCD;
    p[18] = 0x30;
    p[19] = 0x0F;
    p[20] = 0x0B;
    p[21] = 'h';
    p[22] = 'i';
    p[23] = 0;

    enter_ready = 1;
    return row + 1;
}

static int copy_from_user(char *dst, uint64_t src, uint64_t max)
{
    uint64_t i;
    const volatile uint8_t *p;

    if (max == 0 || src < USER_CODE || src >= USER_STACK_TOP) {
        return -1;
    }

    p = (const volatile uint8_t *)(uintptr_t)src;
    for (i = 0; i < max; i++) {
        if (src + i >= USER_STACK_TOP) {
            return -1;
        }
        dst[i] = (char)p[i];
        if (dst[i] == '\0') {
            return 0;
        }
    }
    return -1;
}

static void __attribute__((noreturn)) kernel_idle(void)
{
    __asm__ volatile ("sti");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void user_on_syscall(uint64_t cs, uint64_t nr, uint64_t arg)
{
    char buf[USER_STR_MAX];

    if ((cs & 3ull) != 3ull) {
        vga_write_at(user_row, 0, "user fail");
        return;
    }
    if (nr == SYS_WRITE) {
        if (copy_from_user(buf, arg, USER_STR_MAX) != 0) {
            vga_write_at(user_row, 0, "user fail");
            return;
        }
        vga_write_at(user_row, 0, "        ");
        vga_write_at(user_row, 0, buf);
        return;
    }
    if (nr == SYS_EXIT) {
        vga_write_at(user_row, 3, "exit");
        kernel_idle();
    }
    vga_write_at(user_row, 0, "user fail");
}

__attribute__((noreturn))
void user_enter(void)
{
    uint64_t rip = USER_CODE;
    uint64_t rsp = USER_STACK_TOP;
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
