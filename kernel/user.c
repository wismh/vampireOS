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
 * Every ELF links at the same BASE; each task maps it in its own CR3. */
#define USER_SLOT 0x2000ull
#define USER_BASE 0x400000ull
#define USER_ARG_PATH (USER_BASE + PAGE_4K)
#define KERNEL_STACK_MIN 0x200000ull
#define SYS_WRITE 1ull
#define SYS_EXIT 2ull
#define SYS_YIELD 3ull
#define SYS_SLEEP 4ull
#define SYS_WAIT 5ull
#define SYS_OPEN 6ull
#define SYS_CLOSE 7ull
#define SYS_READ 8ull
#define USER_STR_MAX 80ull
#define FILE_MAX 0x1000ull
#define FD_CONSOLE 1

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
static uint64_t enter_rip;
static uint8_t file_buf[FILE_MAX];

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

/* Map code+stack at BASE into cr3, load ELF into the code page. */
static int map_load_elf(const void *data, unsigned len, uint64_t base,
                        uint64_t cr3, uint64_t *entry, uint64_t *stack_top)
{
    uint64_t code;
    uint64_t stack;
    uint8_t *dest;

    if (cr3 == 0) {
        return -1;
    }
    code = alloc_page();
    stack = alloc_page();
    if (code == 0 || stack == 0) {
        return -1;
    }
    dest = (uint8_t *)(uintptr_t)phys_to_virt(code);
    if (elf_load(data, len, dest, (unsigned)PAGE_4K, base, entry) != 0) {
        return -1;
    }
    if (vmm_map_user(cr3, base, code) != 0 ||
        vmm_map_user(cr3, base + PAGE_4K, stack) != 0) {
        return -1;
    }
    *stack_top = user_stack_top(base);
    return 0;
}

static int map_load_elf_name(const char *name, uint64_t base, uint64_t cr3,
                             uint64_t *entry, uint64_t *stack_top)
{
    const void *data;
    unsigned len;

    if (fs_lookup(name, &data, &len) != 0) {
        return -1;
    }
    return map_load_elf(data, len, base, cr3, entry, stack_top);
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
    uint64_t cr3_a;
    uint64_t cr3_b;
    uint64_t cr3_c;
    uint16_t tr = TSS_SEL;

    enter_ready = 0;
    enter_rip = USER_BASE;
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

    cr3_a = vmm_clone_pml4();
    cr3_b = vmm_clone_pml4();
    cr3_c = vmm_clone_pml4();
    if (cr3_a == 0 || cr3_b == 0 || cr3_c == 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    if (map_load_elf_name("a", USER_BASE, cr3_a, &entry_a, &stack_a) != 0 ||
        map_load_elf_name("b", USER_BASE, cr3_b, &entry_b, &stack_b) != 0 ||
        map_load_elf_name("c", USER_BASE, cr3_c, &entry_c, &stack_c) != 0) {
        vga_write_at(row, 0, "user fail");
        return row + 1;
    }

    sched_init();
    if (sched_add_user(entry_a, stack_a, ktop_a, row + 1, USER_BASE, cr3_a) != 0 ||
        sched_add_user(entry_b, stack_b, ktop_b, row + 2, USER_BASE, cr3_b) != 0 ||
        sched_add_user(entry_c, stack_c, ktop_c, row + 3, USER_BASE, cr3_c) != 0) {
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
    return row + 5;
}

/* Resolve each byte through the current task's PML4 (not %cr3).
 * Kernel VAs and unmapped pages fail; only present user PTEs copy. */
static int copy_from_user(char *dst, uint64_t src, uint64_t max)
{
    uint64_t i;
    uint64_t cr3;
    uint64_t phys;
    uint64_t page_base;
    const volatile uint8_t *page;

    if (max == 0 || dst == 0) {
        return -1;
    }

    cr3 = sched_current_cr3();
    page_base = ~0ull;
    page = 0;
    for (i = 0; i < max; i++) {
        uint64_t va = src + i;
        uint64_t base = va & ~(PAGE_4K - 1);

        if (base != page_base) {
            if (vmm_translate_user(cr3, va, &phys) != 0) {
                return -1;
            }
            page = (const volatile uint8_t *)(uintptr_t)phys_to_virt(phys & ~(PAGE_4K - 1));
            page_base = base;
        }
        dst[i] = (char)page[va & (PAGE_4K - 1)];
        if (dst[i] == '\0') {
            return 0;
        }
    }
    return -1;
}

/* Copy n bytes from user VA into dst (no NUL stop). Returns 0 or -1. */
static int copy_from_user_n(void *dst, uint64_t src, uint64_t n)
{
    uint64_t i;
    uint64_t cr3;
    uint64_t phys;
    uint64_t page_base;
    const volatile uint8_t *page;
    uint8_t *out;

    if (n == 0) {
        return 0;
    }
    if (dst == 0) {
        return -1;
    }

    cr3 = sched_current_cr3();
    page_base = ~0ull;
    page = 0;
    out = (uint8_t *)dst;
    for (i = 0; i < n; i++) {
        uint64_t va = src + i;
        uint64_t base = va & ~(PAGE_4K - 1);

        if (base != page_base) {
            if (vmm_translate_user(cr3, va, &phys) != 0) {
                return -1;
            }
            page = (const volatile uint8_t *)(uintptr_t)phys_to_virt(phys & ~(PAGE_4K - 1));
            page_base = base;
        }
        out[i] = page[va & (PAGE_4K - 1)];
    }
    return 0;
}

/* Write n bytes into user VA in the given PML4. */
static int copy_to_user_pml4(uint64_t cr3, uint64_t dst, const void *src,
                             uint64_t n)
{
    uint64_t i;
    uint64_t phys;
    uint64_t page_base;
    volatile uint8_t *page;
    const uint8_t *in;

    if (n == 0) {
        return 0;
    }
    if (src == 0 || cr3 == 0) {
        return -1;
    }

    page_base = ~0ull;
    page = 0;
    in = (const uint8_t *)src;
    for (i = 0; i < n; i++) {
        uint64_t va = dst + i;
        uint64_t base = va & ~(PAGE_4K - 1);

        if (base != page_base) {
            if (vmm_translate_user(cr3, va, &phys) != 0) {
                return -1;
            }
            page = (volatile uint8_t *)(uintptr_t)phys_to_virt(phys & ~(PAGE_4K - 1));
            page_base = base;
        }
        page[va & (PAGE_4K - 1)] = in[i];
    }
    return 0;
}

/* Write n bytes into user VA through present+user PTEs. */
static int copy_to_user(uint64_t dst, const void *src, uint64_t n)
{
    return copy_to_user_pml4(sched_current_cr3(), dst, src, n);
}

int user_run_path(const char *name, const char *path)
{
    const void *data;
    unsigned len;
    uint64_t base;
    uint64_t entry;
    uint64_t kstack;
    uint64_t ktop;
    uint64_t stack_top;
    uint64_t cr3;
    int row;
    unsigned plen;

    if (!enter_ready || name == 0 || *name == '\0') {
        return -1;
    }
    if (fs_lookup(name, &data, &len) != 0) {
        return -1;
    }
    if (elf_image_base(data, len, &base) != 0) {
        return -1;
    }
    if (base != USER_BASE) {
        return -1;
    }
    row = run_row;
    kstack = alloc_page();
    if (kstack == 0) {
        return -1;
    }
    cr3 = vmm_clone_pml4();
    if (cr3 == 0) {
        return -1;
    }
    if (map_load_elf(data, len, base, cr3, &entry, &stack_top) != 0) {
        pmm_free(cr3);
        return -1;
    }
    if (path != 0 && path[0] != '\0') {
        plen = 0;
        while (path[plen] != '\0' && plen + 1u < FD_PATH_MAX) {
            plen++;
        }
        if (path[plen] != '\0' || plen == 0 ||
            copy_to_user_pml4(cr3, USER_ARG_PATH, path, plen + 1) != 0) {
            vmm_teardown_user(cr3);
            pmm_free(cr3);
            return -1;
        }
    }
    ktop = phys_to_virt(kstack) + PAGE_4K;
    if (sched_add_user(entry, stack_top, ktop, row, base, cr3) != 0) {
        vmm_teardown_user(cr3);
        pmm_free(cr3);
        return -1;
    }
    return 0;
}

int user_run(const char *name)
{
    return user_run_path(name, 0);
}

void user_on_syscall(struct interrupt_frame *frame)
{
    char buf[USER_STR_MAX];
    char path[FD_PATH_MAX];
    int row;
    unsigned n;
    unsigned want;
    unsigned got;
    int fd;
    const void *data;
    unsigned len;

    if (frame == 0 || (frame->cs & 3ull) != 3ull) {
        vga_write_at(user_row, 0, "user fail");
        return;
    }
    if (frame->rax == SYS_WRITE) {
        /* rdi < FD_MAX: fd write (rsi=buf, rdx=len). Else legacy VGA string. */
        if (frame->rdi < (uint64_t)FD_MAX) {
            fd = (int)frame->rdi;
            want = (unsigned)frame->rdx;
            if (want > FILE_MAX) {
                want = (unsigned)FILE_MAX;
            }
            if (fd == FD_CONSOLE) {
                if (want >= USER_STR_MAX) {
                    want = (unsigned)USER_STR_MAX - 1u;
                }
                if (copy_from_user_n(buf, frame->rsi, want) != 0) {
                    frame->rax = (uint64_t)-1;
                    return;
                }
                buf[want] = '\0';
                vmm_set_cr3(vmm_boot_cr3());
                row = sched_row();
                n = sched_note_write();
                if (n <= 8u || (n & 31u) == 0) {
                    vga_write_at(row, 0, buf);
                    vga_write_at(row, (int)want, "          ");
                    vga_write_dec_at(row, (int)want + 1, n);
                }
                frame->rax = (uint64_t)want;
                vmm_set_cr3(sched_current_cr3());
                return;
            }
            if (sched_fd_path(fd, path) != 0) {
                frame->rax = (uint64_t)-1;
                return;
            }
            if (want == 0) {
                frame->rax = 0;
                return;
            }
            if (copy_from_user_n(file_buf, frame->rsi, want) != 0) {
                frame->rax = (uint64_t)-1;
                return;
            }
            vmm_set_cr3(vmm_boot_cr3());
            if (fs_write(path, file_buf, want) != 0) {
                frame->rax = (uint64_t)-1;
                vmm_set_cr3(sched_current_cr3());
                return;
            }
            frame->rax = (uint64_t)want;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        /* Walk task PTEs via HHDM; works even if %cr3 is already boot. */
        if (copy_from_user(buf, frame->rdi, USER_STR_MAX) != 0) {
            vga_write_at(user_row, 0, "user fail");
            return;
        }
        vmm_set_cr3(vmm_boot_cr3());
        row = sched_row();
        n = sched_note_write();
        if (n <= 8u || (n & 31u) == 0) {
            unsigned slen;

            for (slen = 0; slen < USER_STR_MAX && buf[slen] != '\0'; slen++) {
            }
            vga_write_at(row, 0, buf);
            vga_write_at(row, (int)slen, "          ");
            vga_write_dec_at(row, (int)slen + 1, n);
        }
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    if (frame->rax == SYS_READ) {
        fd = (int)frame->rdi;
        want = (unsigned)frame->rdx;
        if (want > FILE_MAX) {
            want = (unsigned)FILE_MAX;
        }
        if (sched_fd_path(fd, path) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        vmm_set_cr3(vmm_boot_cr3());
        if (fs_lookup(path, &data, &len) != 0) {
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        if (len > FILE_MAX) {
            len = (unsigned)FILE_MAX;
        }
        got = want;
        if (got > len) {
            got = len;
        }
        {
            unsigned i;
            const uint8_t *src = (const uint8_t *)data;

            for (i = 0; i < got; i++) {
                file_buf[i] = src[i];
            }
        }
        vmm_set_cr3(sched_current_cr3());
        if (copy_to_user(frame->rsi, file_buf, got) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        frame->rax = (uint64_t)got;
        return;
    }
    if (frame->rax == SYS_OPEN) {
        if (copy_from_user(buf, frame->rdi, USER_STR_MAX) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        vmm_set_cr3(vmm_boot_cr3());
        if (fs_lookup(buf, &data, &len) != 0) {
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        fd = sched_fd_open(buf);
        frame->rax = (fd < 0) ? (uint64_t)-1 : (uint64_t)(unsigned)fd;
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    /* Kernel/idle map for the rest of the syscall body. */
    vmm_set_cr3(vmm_boot_cr3());
    if (frame->rax == SYS_CLOSE) {
        fd = (int)frame->rdi;
        frame->rax = (sched_fd_close(fd) != 0) ? (uint64_t)-1 : 0;
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    if (frame->rax == SYS_EXIT) {
        sched_exit(frame);
        return;
    }
    if (frame->rax == SYS_YIELD) {
        sched_yield(frame);
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    if (frame->rax == SYS_SLEEP) {
        sched_sleep(frame, frame->rdi);
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    if (frame->rax == SYS_WAIT) {
        sched_wait(frame);
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    vga_write_at(user_row, 0, "user fail");
    vmm_set_cr3(sched_current_cr3());
}

__attribute__((noreturn))
void user_enter(void)
{
    uint64_t rip = enter_rip;
    uint64_t rsp = user_stack_top(USER_BASE);
    uint64_t rflags = 0x202;

    if (!enter_ready) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    vmm_set_cr3(sched_current_cr3());
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
