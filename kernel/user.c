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
 * Heap starts at BASE+0x2000 (brk). Every ELF links at the same BASE. */
#define USER_SLOT 0x2000ull
#define USER_BASE 0x400000ull
#define USER_ARGV_MAX 4u
#define KERNEL_STACK_MIN 0x200000ull
#define SYS_WRITE 1ull
#define SYS_EXIT 2ull
#define SYS_YIELD 3ull
#define SYS_SLEEP 4ull
#define SYS_WAIT 5ull
#define SYS_OPEN 6ull
#define SYS_CLOSE 7ull
#define SYS_READ 8ull
#define SYS_READDIR 9ull
#define SYS_EXEC 10ull
#define SYS_PIPE 11ull
#define SYS_BRK 12ull
#define SYS_FORK 13ull
#define SYS_DUP2 14ull
#define USER_HEAP_PAGES 16ull
#define USER_STR_MAX 80ull
#define FILE_MAX 0x1000ull
#define FD_CONSOLE 1
#define CWD_PATH_MAX 80u

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
static unsigned cwd_saved_cl;
static char cwd_saved_path[CWD_PATH_MAX];
static int cwd_switched;

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

static int user_push_argv(uint64_t cr3, uint64_t stack_top, const char **argv,
                          unsigned argc, uint64_t *rsp_out);

static void copy_cwd_path(char *dst, unsigned max, const char *src)
{
    unsigned i;

    if (dst == 0 || max == 0) {
        return;
    }
    if (src == 0 || src[0] == '\0') {
        dst[0] = '/';
        if (max > 1u) {
            dst[1] = '\0';
        }
        return;
    }
    i = 0;
    while (src[i] != '\0' && i + 1u < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Switch the FAT view to this task's cwd. Kernel/shell cwd is restored by
 * leave_task_cwd. No-op when they already match (boot tasks at root). */
static int enter_task_cwd(void)
{
    unsigned cl;
    const char *pwd;

    cwd_switched = 0;
    cl = sched_current_cwd();
    cwd_saved_cl = fs_cwd();
    pwd = fs_pwd();
    copy_cwd_path(cwd_saved_path, CWD_PATH_MAX, pwd);
    if (cl == cwd_saved_cl) {
        return 0;
    }
    if (fs_setcwd(cl) != 0) {
        (void)fs_setcwd(cwd_saved_cl);
        (void)fs_setpwd(cwd_saved_path);
        return -1;
    }
    cwd_switched = 1;
    return 0;
}

static void leave_task_cwd(void)
{
    if (cwd_switched == 0) {
        return;
    }
    cwd_switched = 0;
    (void)fs_setcwd(cwd_saved_cl);
    (void)fs_setpwd(cwd_saved_path);
}

/* Cwd may be a subdir; ELFs live at the volume root. */
static int lookup_root_elf(const char *name, const void **data, unsigned *len)
{
    char rooted[FD_PATH_MAX];
    unsigned i;

    if (name == 0 || *name == '\0' || data == 0 || len == 0) {
        return -1;
    }
    if (fs_lookup(name, data, len) == 0) {
        return 0;
    }
    if (name[0] == '/') {
        return -1;
    }
    rooted[0] = '/';
    i = 0;
    while (name[i] != '\0' && i + 2u < (unsigned)FD_PATH_MAX) {
        rooted[i + 1u] = name[i];
        i++;
    }
    if (name[i] != '\0') {
        return -1;
    }
    rooted[i + 1u] = '\0';
    return fs_lookup(rooted, data, len);
}

/* Alloc new code+stack, then drop the old user map in this CR3. 0 ok, -1 keep
 * old image, -2 old image gone. */
static int replace_load_elf(const void *data, unsigned len, uint64_t base,
                            uint64_t cr3, uint64_t *entry, uint64_t *stack_top)
{
    uint64_t code;
    uint64_t stack;
    uint8_t *dest;

    if (cr3 == 0 || cr3 == vmm_boot_cr3() || entry == 0 || stack_top == 0) {
        return -1;
    }
    code = alloc_page();
    stack = alloc_page();
    if (code == 0 || stack == 0) {
        if (code != 0) {
            pmm_free(code);
        }
        if (stack != 0) {
            pmm_free(stack);
        }
        return -1;
    }
    dest = (uint8_t *)(uintptr_t)phys_to_virt(code);
    if (elf_load(data, len, dest, (unsigned)PAGE_4K, base, entry) != 0) {
        pmm_free(code);
        pmm_free(stack);
        return -1;
    }
    vmm_teardown_user(cr3);
    if (vmm_map_user(cr3, base, code) != 0 ||
        vmm_map_user(cr3, base + PAGE_4K, stack) != 0) {
        return -2;
    }
    *stack_top = user_stack_top(base);
    return 0;
}

/* Load name over the current slot. 0 = frame armed, -1 = old image kept, -2 = gone. */
static int user_exec_current(struct interrupt_frame *frame, const char *name)
{
    const void *data;
    unsigned len;
    uint64_t base;
    uint64_t entry;
    uint64_t stack_top;
    uint64_t cr3;
    const char *argv[1];
    unsigned i;
    int rc;

    if (frame == 0 || name == 0 || *name == '\0') {
        return -1;
    }
    cr3 = sched_current_cr3();
    if (cr3 == 0 || cr3 == vmm_boot_cr3()) {
        return -1;
    }
    if (enter_task_cwd() != 0) {
        return -1;
    }
    if (lookup_root_elf(name, &data, &len) != 0) {
        leave_task_cwd();
        return -1;
    }
    if (elf_image_base(data, len, &base) != 0 || base != USER_BASE) {
        leave_task_cwd();
        return -1;
    }
    if (len == 0 || len > FILE_MAX) {
        leave_task_cwd();
        return -1;
    }
    {
        const uint8_t *src = (const uint8_t *)data;

        for (i = 0; i < len; i++) {
            file_buf[i] = src[i];
        }
    }
    leave_task_cwd();
    rc = replace_load_elf(file_buf, len, base, cr3, &entry, &stack_top);
    if (rc != 0) {
        return rc;
    }
    argv[0] = name;
    if (user_push_argv(cr3, stack_top, argv, 1u, &stack_top) != 0) {
        vmm_teardown_user(cr3);
        return -2;
    }
    sched_reset_current(frame, entry, stack_top, base);
    return 0;
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

/* SysV-ish: argc, argv[0..argc-1], NULL, then string bytes toward stack top. */
static int user_push_argv(uint64_t cr3, uint64_t stack_top, const char **argv,
                          unsigned argc, uint64_t *rsp_out)
{
    uint64_t str_va[USER_ARGV_MAX];
    uint64_t sp;
    uint64_t stack_page;
    uint64_t vec_bytes;
    uint64_t argc64;
    uint64_t zero;
    unsigned i;
    unsigned len;

    if (cr3 == 0 || stack_top == 0 || rsp_out == 0 || argv == 0) {
        return -1;
    }
    if (argc == 0 || argc > USER_ARGV_MAX) {
        return -1;
    }

    stack_page = stack_top - PAGE_4K;
    sp = stack_top;
    for (i = 0; i < argc; i++) {
        if (argv[i] == 0) {
            return -1;
        }
        len = 0;
        while (argv[i][len] != '\0' && len + 1u < FD_PATH_MAX) {
            len++;
        }
        if (argv[i][len] != '\0' || len == 0) {
            return -1;
        }
        if (sp < stack_page + (uint64_t)len + 1ull) {
            return -1;
        }
        sp -= (uint64_t)len + 1ull;
        if (copy_to_user_pml4(cr3, sp, argv[i], (uint64_t)len + 1ull) != 0) {
            return -1;
        }
        str_va[i] = sp;
    }

    sp &= ~7ull;
    vec_bytes = (uint64_t)(argc + 2u) * 8ull;
    if (sp < stack_page + vec_bytes) {
        return -1;
    }
    sp -= vec_bytes;

    argc64 = (uint64_t)argc;
    zero = 0;
    if (copy_to_user_pml4(cr3, sp, &argc64, 8) != 0) {
        return -1;
    }
    for (i = 0; i < argc; i++) {
        if (copy_to_user_pml4(cr3, sp + 8ull * (i + 1u), &str_va[i], 8) != 0) {
            return -1;
        }
    }
    if (copy_to_user_pml4(cr3, sp + 8ull * (argc + 1u), &zero, 8) != 0) {
        return -1;
    }

    *rsp_out = sp;
    return 0;
}

static int user_run_fds(const char *name, const char *arg, int in_pipe,
                        int out_pipe);

int user_run(const char *name, const char *arg)
{
    return user_run_fds(name, arg, -1, -1);
}

static int user_run_fds(const char *name, const char *arg, int in_pipe,
                        int out_pipe)
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
    const char *argv[2];
    unsigned argc;

    if (!enter_ready || name == 0 || *name == '\0') {
        return -1;
    }
    if (lookup_root_elf(name, &data, &len) != 0) {
        return -1;
    }
    if (elf_image_base(data, len, &base) != 0) {
        return -1;
    }
    if (base != USER_BASE) {
        return -1;
    }
    argv[0] = name;
    argc = 1;
    if (arg != 0 && arg[0] != '\0') {
        argv[1] = arg;
        argc = 2;
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
    if (user_push_argv(cr3, stack_top, argv, argc, &stack_top) != 0) {
        vmm_teardown_user(cr3);
        pmm_free(cr3);
        return -1;
    }
    ktop = phys_to_virt(kstack) + PAGE_4K;
    if (sched_add_user(entry, stack_top, ktop, row, base, cr3) != 0) {
        vmm_teardown_user(cr3);
        pmm_free(cr3);
        return -1;
    }
    if (in_pipe >= 0 &&
        sched_fd_bind_pipe(0, FD_KIND_PIPE_R, in_pipe) != 0) {
        return -1;
    }
    if (out_pipe >= 0 &&
        sched_fd_bind_pipe(1, FD_KIND_PIPE_W, out_pipe) != 0) {
        return -1;
    }
    return 0;
}

int user_run_pipeline(const char *left_name, const char *left_arg,
                      const char *right_name, const char *right_arg)
{
    int p;

    p = sched_pipe_new();
    if (p < 0) {
        return -1;
    }
    if (user_run_fds(left_name, left_arg, -1, p) != 0) {
        sched_pipe_unused(p);
        return -1;
    }
    if (user_run_fds(right_name, right_arg, p, -1) != 0) {
        sched_pipe_unused(p);
        return -1;
    }
    return 0;
}

static uint64_t page_up(uint64_t addr)
{
    return (addr + PAGE_4K - 1ull) & ~(PAGE_4K - 1ull);
}

/* Heap starts just after the stack page (BASE+0x2000). rdi=0 queries. */
static uint64_t user_brk(uint64_t want)
{
    uint64_t cr3;
    uint64_t base;
    uint64_t start;
    uint64_t max;
    uint64_t old;
    uint64_t old_end;
    uint64_t new_end;
    uint64_t va;
    uint64_t phys;

    cr3 = sched_current_cr3();
    base = sched_current_base();
    if (cr3 == 0 || cr3 == vmm_boot_cr3() || base == 0) {
        return (uint64_t)-1;
    }
    start = base + 2ull * PAGE_4K;
    max = start + USER_HEAP_PAGES * PAGE_4K;
    old = sched_current_brk();
    if (old < start || old > max) {
        old = start;
    }
    if (want == 0) {
        return old;
    }
    if (want < start || want > max) {
        return (uint64_t)-1;
    }
    old_end = page_up(old);
    new_end = page_up(want);
    if (new_end > old_end) {
        for (va = old_end; va < new_end; va += PAGE_4K) {
            phys = alloc_page();
            if (phys == 0 || vmm_map_user(cr3, va, phys) != 0) {
                if (phys != 0) {
                    pmm_free(phys);
                }
                while (va > old_end) {
                    va -= PAGE_4K;
                    (void)vmm_unmap_user(cr3, va);
                }
                return (uint64_t)-1;
            }
        }
    } else if (new_end < old_end) {
        for (va = new_end; va < old_end; va += PAGE_4K) {
            if (vmm_unmap_user(cr3, va) != 0) {
                return (uint64_t)-1;
            }
        }
    }
    sched_set_brk(want);
    return want;
}

/* Eager copy: cloned PML4, copied user pages (heap included), new kstack. */
static uint64_t user_fork(struct interrupt_frame *frame)
{
    uint64_t src_cr3;
    uint64_t cr3;
    uint64_t kstack;
    uint64_t ktop;
    int id;

    if (frame == 0) {
        return (uint64_t)-1;
    }
    src_cr3 = sched_current_cr3();
    if (src_cr3 == 0 || src_cr3 == vmm_boot_cr3()) {
        return (uint64_t)-1;
    }
    kstack = alloc_page();
    if (kstack == 0) {
        return (uint64_t)-1;
    }
    cr3 = vmm_clone_pml4();
    if (cr3 == 0) {
        pmm_free(kstack);
        return (uint64_t)-1;
    }
    if (vmm_copy_user(cr3, src_cr3) != 0) {
        vmm_teardown_user(cr3);
        pmm_free(cr3);
        pmm_free(kstack);
        return (uint64_t)-1;
    }
    ktop = phys_to_virt(kstack) + PAGE_4K;
    id = sched_fork(frame, ktop, cr3);
    if (id < 0) {
        vmm_teardown_user(cr3);
        pmm_free(cr3);
        pmm_free(kstack);
        return (uint64_t)-1;
    }
    return (uint64_t)(unsigned)id;
}

void user_on_syscall(struct interrupt_frame *frame)
{
    char buf[USER_STR_MAX];
    char path[FD_PATH_MAX];
    int row;
    int packed;
    unsigned n;
    unsigned want;
    unsigned got;
    int fd;
    int kind;
    int pipefd[2];
    const void *data;
    unsigned len;

    if (frame == 0 || (frame->cs & 3ull) != 3ull) {
        vga_write_at(user_row, 0, "user fail");
        return;
    }
    if (frame->rax == SYS_WRITE) {
        /* rdi < FD_MAX: fd write (rsi=buf, rdx=len). Else legacy VGA string.
         * Pipe ends are checked first so fd 1 can be a pipe write, not VGA. */
        if (frame->rdi < (uint64_t)FD_MAX) {
            fd = (int)frame->rdi;
            want = (unsigned)frame->rdx;
            if (want > FILE_MAX) {
                want = (unsigned)FILE_MAX;
            }
            kind = sched_fd_kind(fd);
            if (kind == FD_KIND_PIPE_W) {
                if (want == 0) {
                    frame->rax = 0;
                    return;
                }
                if (copy_from_user_n(file_buf, frame->rsi, want) != 0) {
                    frame->rax = (uint64_t)-1;
                    return;
                }
                packed = sched_pipe_write(fd, file_buf, want);
                if (packed == -2) {
                    frame->rip -= 2ull;
                    sched_block_pipe(frame, sched_fd_pipe(fd));
                    return;
                }
                frame->rax = (packed < 0) ? (uint64_t)-1 : (uint64_t)(unsigned)packed;
                return;
            }
            if (kind == FD_KIND_PIPE_R) {
                frame->rax = (uint64_t)-1;
                return;
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
            if (enter_task_cwd() != 0 || fs_write(path, file_buf, want) != 0) {
                leave_task_cwd();
                frame->rax = (uint64_t)-1;
                vmm_set_cr3(sched_current_cr3());
                return;
            }
            leave_task_cwd();
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
        kind = sched_fd_kind(fd);
        if (kind == FD_KIND_PIPE_R) {
            if (want == 0) {
                frame->rax = 0;
                return;
            }
            packed = sched_pipe_read(fd, file_buf, want);
            if (packed == -2) {
                frame->rip -= 2ull;
                sched_block_pipe(frame, sched_fd_pipe(fd));
                return;
            }
            if (packed < 0) {
                frame->rax = (uint64_t)-1;
                return;
            }
            got = (unsigned)packed;
            if (got != 0 && copy_to_user(frame->rsi, file_buf, got) != 0) {
                frame->rax = (uint64_t)-1;
                return;
            }
            frame->rax = (uint64_t)got;
            return;
        }
        if (kind == FD_KIND_PIPE_W) {
            frame->rax = (uint64_t)-1;
            return;
        }
        if (sched_fd_path(fd, path) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        vmm_set_cr3(vmm_boot_cr3());
        if (enter_task_cwd() != 0 || fs_lookup(path, &data, &len) != 0) {
            leave_task_cwd();
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
        leave_task_cwd();
        vmm_set_cr3(sched_current_cr3());
        if (copy_to_user(frame->rsi, file_buf, got) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        frame->rax = (uint64_t)got;
        return;
    }
    if (frame->rax == SYS_READDIR) {
        want = (unsigned)frame->rsi;
        if (want > FILE_MAX) {
            want = (unsigned)FILE_MAX;
        }
        vmm_set_cr3(vmm_boot_cr3());
        if (enter_task_cwd() != 0) {
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        packed = fs_readdir((char *)file_buf, want);
        leave_task_cwd();
        if (packed < 0) {
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        got = (unsigned)packed;
        vmm_set_cr3(sched_current_cr3());
        if (got != 0 && copy_to_user(frame->rdi, file_buf, got) != 0) {
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
        if (enter_task_cwd() != 0 || fs_lookup(buf, &data, &len) != 0) {
            leave_task_cwd();
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
        leave_task_cwd();
        fd = sched_fd_open(buf);
        frame->rax = (fd < 0) ? (uint64_t)-1 : (uint64_t)(unsigned)fd;
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    /* pipe(fd[2]): rdi = user int[2]; fd[0] read, fd[1] write; rax 0 or -1. */
    if (frame->rax == SYS_PIPE) {
        if (sched_pipe(pipefd) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        if (copy_to_user(frame->rdi, pipefd, sizeof(pipefd)) != 0) {
            sched_fd_close(pipefd[0]);
            sched_fd_close(pipefd[1]);
            frame->rax = (uint64_t)-1;
            return;
        }
        frame->rax = 0;
        return;
    }
    if (frame->rax == SYS_EXEC) {
        if (copy_from_user(buf, frame->rdi, USER_STR_MAX) != 0) {
            frame->rax = (uint64_t)-1;
            return;
        }
        n = 0;
        while (buf[n] != '\0') {
            n++;
        }
        if (n == 0 || n >= (unsigned)FD_PATH_MAX) {
            frame->rax = (uint64_t)-1;
            return;
        }
        vmm_set_cr3(vmm_boot_cr3());
        packed = user_exec_current(frame, buf);
        if (packed == -2) {
            frame->rdi = 0;
            sched_exit(frame);
            return;
        }
        if (packed != 0) {
            frame->rax = (uint64_t)-1;
            vmm_set_cr3(sched_current_cr3());
            return;
        }
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
    if (frame->rax == SYS_BRK) {
        frame->rax = user_brk(frame->rdi);
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    if (frame->rax == SYS_FORK) {
        frame->rax = user_fork(frame);
        vmm_set_cr3(sched_current_cr3());
        return;
    }
    /* rdi=oldfd, rsi=newfd; rax=newfd or -1. Source stays; target replaced. */
    if (frame->rax == SYS_DUP2) {
        packed = sched_fd_dup2((int)frame->rdi, (int)frame->rsi);
        frame->rax = (packed < 0) ? (uint64_t)-1 : (uint64_t)(unsigned)packed;
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
