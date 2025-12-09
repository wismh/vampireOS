#include "user.h"
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
#define USER_CODE 0x400000ull
#define USER_STACK 0x401000ull
#define USER_STACK_TOP 0x402000ull
#define USER_B_CODE 0x402000ull
#define USER_B_STACK 0x403000ull
#define USER_B_STACK_TOP 0x404000ull
#define USER_C_CODE 0x404000ull
#define USER_C_STACK 0x405000ull
#define USER_C_STACK_TOP 0x406000ull
#define USER_LIMIT 0x406000ull
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

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}

int user_ready(void)
{
    return enter_ready;
}

static void emit_wait_then_loop(uint8_t *p, uint32_t str_va, char letter)
{
    p[0] = 0xB8;
    p[1] = 0x05;
    p[2] = 0x00;
    p[3] = 0x00;
    p[4] = 0x00;
    p[5] = 0xCD;
    p[6] = 0x30;
    p[7] = 0xB8;
    p[8] = 0x01;
    p[9] = 0x00;
    p[10] = 0x00;
    p[11] = 0x00;
    p[12] = 0xBF;
    p[13] = (uint8_t)str_va;
    p[14] = (uint8_t)(str_va >> 8);
    p[15] = (uint8_t)(str_va >> 16);
    p[16] = (uint8_t)(str_va >> 24);
    p[17] = 0xCD;
    p[18] = 0x30;
    p[19] = 0xB8;
    p[20] = 0x03;
    p[21] = 0x00;
    p[22] = 0x00;
    p[23] = 0x00;
    p[24] = 0xBF;
    p[25] = 0x00;
    p[26] = 0x00;
    p[27] = 0x00;
    p[28] = 0x00;
    p[29] = 0xCD;
    p[30] = 0x30;
    p[31] = 0xEB;
    p[32] = 0xE6;
    p[33] = (uint8_t)letter;
    p[34] = 0;
}

static void emit_n_then_exit(uint8_t *p, uint32_t str_va, char letter, uint8_t n,
                             uint8_t sleep_ticks)
{
    p[0] = 0xB9;
    p[1] = n;
    p[2] = 0x00;
    p[3] = 0x00;
    p[4] = 0x00;
    p[5] = 0xB8;
    p[6] = 0x01;
    p[7] = 0x00;
    p[8] = 0x00;
    p[9] = 0x00;
    p[10] = 0xBF;
    p[11] = (uint8_t)str_va;
    p[12] = (uint8_t)(str_va >> 8);
    p[13] = (uint8_t)(str_va >> 16);
    p[14] = (uint8_t)(str_va >> 24);
    p[15] = 0xCD;
    p[16] = 0x30;
    p[17] = 0xFF;
    p[18] = 0xC9;
    p[19] = 0x74;
    p[20] = 0x0E;
    p[21] = 0xB8;
    p[22] = 0x04;
    p[23] = 0x00;
    p[24] = 0x00;
    p[25] = 0x00;
    p[26] = 0xBF;
    p[27] = sleep_ticks;
    p[28] = 0x00;
    p[29] = 0x00;
    p[30] = 0x00;
    p[31] = 0xCD;
    p[32] = 0x30;
    p[33] = 0xEB;
    p[34] = 0xE2;
    p[35] = 0xB8;
    p[36] = 0x02;
    p[37] = 0x00;
    p[38] = 0x00;
    p[39] = 0x00;
    p[40] = 0xCD;
    p[41] = 0x30;
    p[42] = (uint8_t)letter;
    p[43] = 0;
}

static void emit_write_wait(uint8_t *p, uint32_t str_va, char letter, uint8_t sys,
                            uint32_t arg)
{
    p[0] = 0xB8;
    p[1] = 0x01;
    p[2] = 0x00;
    p[3] = 0x00;
    p[4] = 0x00;
    p[5] = 0xBF;
    p[6] = (uint8_t)str_va;
    p[7] = (uint8_t)(str_va >> 8);
    p[8] = (uint8_t)(str_va >> 16);
    p[9] = (uint8_t)(str_va >> 24);
    p[10] = 0xCD;
    p[11] = 0x30;
    p[12] = 0xB8;
    p[13] = sys;
    p[14] = 0x00;
    p[15] = 0x00;
    p[16] = 0x00;
    p[17] = 0xBF;
    p[18] = (uint8_t)arg;
    p[19] = (uint8_t)(arg >> 8);
    p[20] = (uint8_t)(arg >> 16);
    p[21] = (uint8_t)(arg >> 24);
    p[22] = 0xCD;
    p[23] = 0x30;
    p[24] = 0xEB;
    p[25] = 0xE6;
    p[26] = (uint8_t)letter;
    p[27] = 0;
}

int user_init(int row)
{
    uint64_t kstack_a;
    uint64_t kstack_b;
    uint64_t kstack_c;
    uint64_t ktop_a;
    uint64_t ktop_b;
    uint64_t ktop_c;
    uint64_t code_a;
    uint64_t stack_a;
    uint64_t code_b;
    uint64_t stack_b;
    uint64_t code_c;
    uint64_t stack_c;
    uint8_t *p;
    uint16_t tr = TSS_SEL;

    enter_ready = 0;
    user_row = row;
    if (row >= VGA_HEIGHT - 5) {
        return row;
    }

    if (!gdt_ready) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    kstack_a = alloc_page();
    kstack_b = alloc_page();
    kstack_c = alloc_page();
    code_a = alloc_page();
    stack_a = alloc_page();
    code_b = alloc_page();
    stack_b = alloc_page();
    code_c = alloc_page();
    stack_c = alloc_page();
    if (kstack_a == 0 || kstack_b == 0 || kstack_c == 0 || code_a == 0 ||
        stack_a == 0 || code_b == 0 || stack_b == 0 || code_c == 0 ||
        stack_c == 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    ktop_a = phys_to_virt(kstack_a) + PAGE_4K;
    ktop_b = phys_to_virt(kstack_b) + PAGE_4K;
    ktop_c = phys_to_virt(kstack_c) + PAGE_4K;
    tss_set_rsp0(ktop_a);
    __asm__ volatile ("ltr %0" : : "r"(tr) : "memory");

    if (vmm_map_user(USER_CODE, code_a) != 0 ||
        vmm_map_user(USER_STACK, stack_a) != 0 ||
        vmm_map_user(USER_B_CODE, code_b) != 0 ||
        vmm_map_user(USER_B_STACK, stack_b) != 0 ||
        vmm_map_user(USER_C_CODE, code_c) != 0 ||
        vmm_map_user(USER_C_STACK, stack_c) != 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    p = (uint8_t *)(uintptr_t)phys_to_virt(code_a);
    emit_wait_then_loop(p, (uint32_t)(USER_CODE + 33ull), 'A');
    p = (uint8_t *)(uintptr_t)phys_to_virt(code_b);
    emit_write_wait(p, (uint32_t)(USER_B_CODE + 26ull), 'B', (uint8_t)SYS_SLEEP, 1);
    p = (uint8_t *)(uintptr_t)phys_to_virt(code_c);
    emit_n_then_exit(p, (uint32_t)(USER_C_CODE + 42ull), 'C', 8, 5);

    sched_init();
    if (sched_add_user(USER_CODE, USER_STACK_TOP, ktop_a, row + 1) != 0 ||
        sched_add_user(USER_B_CODE, USER_B_STACK_TOP, ktop_b, row + 2) != 0 ||
        sched_add_user(USER_C_CODE, USER_C_STACK_TOP, ktop_c, row + 3) != 0) {
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

    enter_ready = 1;
    return row + 4;
}

static int copy_from_user(char *dst, uint64_t src, uint64_t max)
{
    uint64_t i;
    const volatile uint8_t *p;

    if (max == 0 || src < USER_CODE || src >= USER_LIMIT) {
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
