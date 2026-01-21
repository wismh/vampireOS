#include "sched.h"
#include "idt.h"
#include "pmm.h"
#include "user.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

#define TASK_MAX 8
#define TASK_DEAD 0
#define TASK_READY 1
#define TASK_SLEEP 2
#define TASK_WAIT 3
#define USER_CS 0x2B
#define USER_DS 0x23
#define PAGE_4K 0x1000ull

struct fd_entry {
    int used;
    char path[FD_PATH_MAX];
};

struct task {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
    uint64_t kstack_top;
    uint64_t user_base;
    uint64_t cr3; /* cloned PML4 phys; loaded on switch */
    int state;
    int row;
    unsigned writes;
    unsigned wake_tick;
    struct fd_entry fds[FD_MAX];
};

static struct task tasks[TASK_MAX];
static int task_count;
static int current;

static void save_task(struct task *t, const struct interrupt_frame *f)
{
    t->rax = f->rax;
    t->rbx = f->rbx;
    t->rcx = f->rcx;
    t->rdx = f->rdx;
    t->rdi = f->rdi;
    t->rsi = f->rsi;
    t->rbp = f->rbp;
    t->r8 = f->r8;
    t->r9 = f->r9;
    t->r10 = f->r10;
    t->r11 = f->r11;
    t->r12 = f->r12;
    t->r13 = f->r13;
    t->r14 = f->r14;
    t->r15 = f->r15;
    t->rip = f->rip;
    t->cs = f->cs;
    t->rflags = f->rflags;
    t->rsp = f->rsp;
    t->ss = f->ss;
}

static void load_task(struct interrupt_frame *f, const struct task *t)
{
    f->rax = t->rax;
    f->rbx = t->rbx;
    f->rcx = t->rcx;
    f->rdx = t->rdx;
    f->rdi = t->rdi;
    f->rsi = t->rsi;
    f->rbp = t->rbp;
    f->r8 = t->r8;
    f->r9 = t->r9;
    f->r10 = t->r10;
    f->r11 = t->r11;
    f->r12 = t->r12;
    f->r13 = t->r13;
    f->r14 = t->r14;
    f->r15 = t->r15;
    f->rip = t->rip;
    f->cs = t->cs;
    f->rflags = t->rflags;
    f->rsp = t->rsp;
    f->ss = t->ss;
    vmm_set_cr3(t->cr3);
}

static int pick_next(int from)
{
    int i;
    int idx;

    for (i = 1; i <= task_count; i++) {
        idx = (from + i) % task_count;
        if (tasks[idx].state == TASK_READY) {
            return idx;
        }
    }
    return -1;
}

static void set_current(int idx)
{
    current = idx;
    tss_set_rsp0(tasks[idx].kstack_top);
}

static void clear_fds(struct task *t)
{
    int i;
    int j;

    if (t == 0) {
        return;
    }
    for (i = 0; i < FD_MAX; i++) {
        t->fds[i].used = 0;
        for (j = 0; j < FD_PATH_MAX; j++) {
            t->fds[i].path[j] = 0;
        }
    }
}

static void copy_path(char *dst, const char *src)
{
    int i;

    if (dst == 0) {
        return;
    }
    for (i = 0; i < FD_PATH_MAX - 1 && src != 0 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/* Unmap and free user code/stack frames in this task's CR3. */
static void free_task_user(struct task *t)
{
    if (t == 0 || t->user_base == 0 || t->cr3 == 0) {
        return;
    }
    vmm_unmap_user(t->cr3, t->user_base);
    vmm_unmap_user(t->cr3, t->user_base + PAGE_4K);
    t->user_base = 0;
}

/* Free a dead task's kernel stack (never while still running on it). */
static void free_task_kstack(struct task *t)
{
    uint64_t kphys;

    if (t == 0 || t->kstack_top == 0) {
        return;
    }
    kphys = virt_to_phys(t->kstack_top - PAGE_4K);
    if (kphys != 0) {
        pmm_free(kphys);
    }
    t->kstack_top = 0;
}

/* Free only the cloned PML4 page (kernel/HHDM lower tables stay shared). */
static void free_task_pml4(struct task *t)
{
    if (t == 0 || t->cr3 == 0) {
        return;
    }
    if (t->cr3 != vmm_boot_cr3()) {
        pmm_free(t->cr3);
    }
    t->cr3 = 0;
}

void sched_init(void)
{
    int i;

    task_count = 0;
    current = 0;
    for (i = 0; i < TASK_MAX; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].writes = 0;
        tasks[i].wake_tick = 0;
        tasks[i].kstack_top = 0;
        tasks[i].user_base = 0;
        tasks[i].cr3 = 0;
        clear_fds(&tasks[i]);
    }
}

int sched_base_busy(uint64_t user_base, uint64_t cr3)
{
    int i;

    if (user_base == 0 || cr3 == 0) {
        return 1;
    }
    for (i = 0; i < task_count; i++) {
        if (tasks[i].state != TASK_DEAD && tasks[i].cr3 == cr3 &&
            tasks[i].user_base == user_base) {
            return 1;
        }
    }
    return 0;
}

int sched_add_user(uint64_t rip, uint64_t rsp, uint64_t kstack_top, int row,
                   uint64_t user_base, uint64_t cr3)
{
    struct task *t;
    int i;

    if (kstack_top == 0 || user_base == 0 || cr3 == 0 ||
        cr3 == vmm_boot_cr3()) {
        return -1;
    }
    if (sched_base_busy(user_base, cr3)) {
        return -1;
    }
    t = 0;
    for (i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_DEAD) {
            t = &tasks[i];
            free_task_user(t);
            free_task_kstack(t);
            free_task_pml4(t);
            break;
        }
    }
    if (t == 0) {
        if (task_count >= TASK_MAX) {
            return -1;
        }
        t = &tasks[task_count];
        if (task_count == 0) {
            current = 0;
        }
        task_count++;
    }
    t->rax = 0;
    t->rbx = 0;
    t->rcx = 0;
    t->rdx = 0;
    t->rdi = 0;
    t->rsi = 0;
    t->rbp = 0;
    t->r8 = 0;
    t->r9 = 0;
    t->r10 = 0;
    t->r11 = 0;
    t->r12 = 0;
    t->r13 = 0;
    t->r14 = 0;
    t->r15 = 0;
    t->rip = rip;
    t->cs = USER_CS;
    t->rflags = 0x202;
    t->rsp = rsp;
    t->ss = USER_DS;
    t->kstack_top = kstack_top;
    t->user_base = user_base;
    t->cr3 = cr3;
    t->state = TASK_READY;
    t->row = row;
    t->writes = 0;
    t->wake_tick = 0;
    clear_fds(t);
    return 0;
}

int sched_fd_open(const char *path)
{
    struct task *t;
    int i;

    if (task_count == 0 || path == 0 || path[0] == '\0') {
        return -1;
    }
    t = &tasks[current];
    for (i = 0; i < FD_MAX; i++) {
        if (t->fds[i].used == 0) {
            t->fds[i].used = 1;
            copy_path(t->fds[i].path, path);
            return i;
        }
    }
    return -1;
}

int sched_fd_close(int fd)
{
    struct task *t;
    int j;

    if (task_count == 0 || fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    t = &tasks[current];
    if (t->fds[fd].used == 0) {
        return -1;
    }
    t->fds[fd].used = 0;
    for (j = 0; j < FD_PATH_MAX; j++) {
        t->fds[fd].path[j] = 0;
    }
    return 0;
}

uint64_t sched_current_cr3(void)
{
    if (task_count == 0 || tasks[current].cr3 == 0) {
        return vmm_boot_cr3();
    }
    return tasks[current].cr3;
}

int sched_row(void)
{
    if (task_count == 0) {
        return 0;
    }
    return tasks[current].row;
}

unsigned sched_note_write(void)
{
    if (task_count == 0) {
        return 0;
    }
    tasks[current].writes++;
    return tasks[current].writes;
}

static void wake_sleepers(void)
{
    int i;
    unsigned now = idt_ticks();

    for (i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_SLEEP && tasks[i].wake_tick <= now) {
            tasks[i].state = TASK_READY;
        }
    }
}

static void wake_waiters(void)
{
    int i;

    for (i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_WAIT) {
            tasks[i].state = TASK_READY;
        }
    }
}

static void idle_until_ready(struct interrupt_frame *frame)
{
    int next;

    vmm_set_cr3(vmm_boot_cr3());
    for (;;) {
        wake_sleepers();
        next = pick_next(current);
        if (next >= 0) {
            set_current(next);
            load_task(frame, &tasks[current]);
            return;
        }
        __asm__ volatile ("sti; hlt; cli" ::: "memory");
    }
}

static void switch_to_next(struct interrupt_frame *frame)
{
    int next;

    if (frame == 0) {
        return;
    }
    next = pick_next(current);
    if (next < 0 || next == current) {
        return;
    }
    save_task(&tasks[current], frame);
    set_current(next);
    load_task(frame, &tasks[current]);
}

void sched_on_tick(struct interrupt_frame *frame)
{
    wake_sleepers();
    if (frame == 0 || (frame->cs & 3ull) != 3ull) {
        return;
    }
    switch_to_next(frame);
}

void sched_yield(struct interrupt_frame *frame)
{
    switch_to_next(frame);
}

void sched_sleep(struct interrupt_frame *frame, uint64_t n)
{
    unsigned now;
    int next;

    if (frame == 0) {
        return;
    }
    if (n == 0) {
        sched_yield(frame);
        return;
    }
    now = idt_ticks();
    save_task(&tasks[current], frame);
    tasks[current].state = TASK_SLEEP;
    tasks[current].wake_tick = now + (unsigned)n;
    next = pick_next(current);
    if (next < 0) {
        idle_until_ready(frame);
        return;
    }
    set_current(next);
    load_task(frame, &tasks[current]);
}

void sched_wait(struct interrupt_frame *frame)
{
    int i;
    int next;

    if (frame == 0) {
        return;
    }
    for (i = 0; i < task_count; i++) {
        if (i != current && tasks[i].state == TASK_DEAD) {
            return;
        }
    }
    save_task(&tasks[current], frame);
    tasks[current].state = TASK_WAIT;
    next = pick_next(current);
    if (next < 0) {
        idle_until_ready(frame);
        return;
    }
    set_current(next);
    load_task(frame, &tasks[current]);
}

void sched_exit(struct interrupt_frame *frame)
{
    int next;
    int row;

    if (task_count != 0) {
        row = tasks[current].row;
        vga_write_at(row, 2, "done");
        /* Still on this task's kstack until iretq; free that on slot reuse. */
        free_task_user(&tasks[current]);
        clear_fds(&tasks[current]);
        tasks[current].state = TASK_DEAD;
        wake_waiters();
    }
    next = pick_next(current);
    if (next < 0 || frame == 0) {
        vmm_set_cr3(vmm_boot_cr3());
        __asm__ volatile ("sti");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
    set_current(next);
    load_task(frame, &tasks[current]);
}
