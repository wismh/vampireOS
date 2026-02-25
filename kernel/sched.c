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
#define TASK_PIPE 4
#define USER_CS 0x2B
#define USER_DS 0x23
#define PAGE_4K 0x1000ull
#define PIPE_MAX 4
#define PIPE_CAP 0x1000u
#define PIPE_MASK (PIPE_CAP - 1u)
#define PIPE_MIN_PHYS 0x200000ull

struct fd_entry {
    int used;
    int kind;
    int pipe;
    char path[FD_PATH_MAX];
};

struct pipe {
    int used;
    unsigned head;
    unsigned len;
    int rrefs;
    int wrefs;
    uint64_t phys;
    uint8_t *data;
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
    uint8_t exit_code;
    uint8_t seen_status;
    uint8_t shown_status;
    int pipe_wait;
    struct fd_entry fds[FD_MAX];
};

static struct task tasks[TASK_MAX];
static struct pipe pipes[PIPE_MAX];
static int task_count;
static int current;
static int last_added;
static uint8_t last_exit;
static int have_exit;

static void idle_until_ready(struct interrupt_frame *frame);

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

static void wake_pipe_waiters(int pipe_id)
{
    int i;

    for (i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_PIPE && tasks[i].pipe_wait == pipe_id) {
            tasks[i].state = TASK_READY;
            tasks[i].pipe_wait = -1;
        }
    }
}

static void free_pipe(int p)
{
    if (p < 0 || p >= PIPE_MAX) {
        return;
    }
    if (pipes[p].phys != 0) {
        pmm_free(pipes[p].phys);
    }
    pipes[p].used = 0;
    pipes[p].head = 0;
    pipes[p].len = 0;
    pipes[p].rrefs = 0;
    pipes[p].wrefs = 0;
    pipes[p].phys = 0;
    pipes[p].data = 0;
}

static int alloc_pipe(void)
{
    int p;
    unsigned i;
    uint64_t phys;
    uint8_t *data;

    p = -1;
    for (i = 0; i < PIPE_MAX; i++) {
        if (pipes[i].used == 0) {
            p = (int)i;
            break;
        }
    }
    if (p < 0) {
        return -1;
    }
    phys = pmm_alloc_above(PIPE_MIN_PHYS);
    if (phys == 0) {
        phys = pmm_alloc();
    }
    if (phys == 0) {
        return -1;
    }
    data = (uint8_t *)(uintptr_t)phys_to_virt(phys);
    for (i = 0; i < PIPE_CAP; i++) {
        data[i] = 0;
    }
    pipes[p].used = 1;
    pipes[p].head = 0;
    pipes[p].len = 0;
    pipes[p].rrefs = 0;
    pipes[p].wrefs = 0;
    pipes[p].phys = phys;
    pipes[p].data = data;
    return p;
}

static void drop_fd(struct fd_entry *e)
{
    int p;
    int j;

    if (e == 0 || e->used == 0) {
        return;
    }
    if ((e->kind == FD_KIND_PIPE_R || e->kind == FD_KIND_PIPE_W) &&
        e->pipe >= 0 && e->pipe < PIPE_MAX && pipes[e->pipe].used != 0) {
        p = e->pipe;
        if (e->kind == FD_KIND_PIPE_R) {
            if (pipes[p].rrefs > 0) {
                pipes[p].rrefs--;
            }
        } else if (pipes[p].wrefs > 0) {
            pipes[p].wrefs--;
        }
        wake_pipe_waiters(p);
        if (pipes[p].rrefs == 0 && pipes[p].wrefs == 0) {
            free_pipe(p);
        }
    }
    e->used = 0;
    e->kind = 0;
    e->pipe = -1;
    for (j = 0; j < FD_PATH_MAX; j++) {
        e->path[j] = 0;
    }
}

static void clear_fds(struct task *t)
{
    int i;

    if (t == 0) {
        return;
    }
    for (i = 0; i < FD_MAX; i++) {
        drop_fd(&t->fds[i]);
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

/* Drop all user PTEs in this task's CR3; PML4 stays until slot reuse. */
static void free_task_user(struct task *t)
{
    if (t == 0 || t->cr3 == 0) {
        return;
    }
    vmm_teardown_user(t->cr3);
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
    last_added = -1;
    last_exit = 0;
    have_exit = 0;
    for (i = 0; i < PIPE_MAX; i++) {
        pipes[i].used = 0;
        pipes[i].head = 0;
        pipes[i].len = 0;
        pipes[i].rrefs = 0;
        pipes[i].wrefs = 0;
        pipes[i].phys = 0;
        pipes[i].data = 0;
    }
    for (i = 0; i < TASK_MAX; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].writes = 0;
        tasks[i].wake_tick = 0;
        tasks[i].exit_code = 0;
        tasks[i].seen_status = 0;
        tasks[i].shown_status = 0;
        tasks[i].pipe_wait = -1;
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
    t->exit_code = 0;
    t->seen_status = 0;
    t->shown_status = 0;
    t->pipe_wait = -1;
    clear_fds(t);
    last_added = (int)(t - tasks);
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
            t->fds[i].kind = FD_KIND_FILE;
            t->fds[i].pipe = -1;
            copy_path(t->fds[i].path, path);
            return i;
        }
    }
    return -1;
}

int sched_fd_close(int fd)
{
    struct task *t;

    if (task_count == 0 || fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    t = &tasks[current];
    if (t->fds[fd].used == 0) {
        return -1;
    }
    drop_fd(&t->fds[fd]);
    return 0;
}

int sched_fd_path(int fd, char *out)
{
    struct task *t;
    int j;

    if (task_count == 0 || fd < 0 || fd >= FD_MAX || out == 0) {
        return -1;
    }
    t = &tasks[current];
    if (t->fds[fd].used == 0 || t->fds[fd].kind != FD_KIND_FILE) {
        return -1;
    }
    for (j = 0; j < FD_PATH_MAX; j++) {
        out[j] = t->fds[fd].path[j];
    }
    return 0;
}

int sched_fd_kind(int fd)
{
    struct task *t;

    if (task_count == 0 || fd < 0 || fd >= FD_MAX) {
        return 0;
    }
    t = &tasks[current];
    if (t->fds[fd].used == 0) {
        return 0;
    }
    return t->fds[fd].kind;
}

int sched_fd_pipe(int fd)
{
    struct task *t;

    if (task_count == 0 || fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    t = &tasks[current];
    if (t->fds[fd].used == 0) {
        return -1;
    }
    if (t->fds[fd].kind != FD_KIND_PIPE_R && t->fds[fd].kind != FD_KIND_PIPE_W) {
        return -1;
    }
    return t->fds[fd].pipe;
}

int sched_pipe(int out[2])
{
    struct task *t;
    int p;
    int r;
    int w;
    unsigned i;

    if (task_count == 0 || out == 0) {
        return -1;
    }
    t = &tasks[current];
    r = -1;
    w = -1;
    for (i = 0; i < (unsigned)FD_MAX; i++) {
        if (t->fds[i].used == 0) {
            if (r < 0) {
                r = (int)i;
            } else {
                w = (int)i;
                break;
            }
        }
    }
    if (r < 0 || w < 0) {
        return -1;
    }
    p = alloc_pipe();
    if (p < 0) {
        return -1;
    }
    pipes[p].rrefs = 1;
    pipes[p].wrefs = 1;
    t->fds[r].used = 1;
    t->fds[r].kind = FD_KIND_PIPE_R;
    t->fds[r].pipe = p;
    t->fds[w].used = 1;
    t->fds[w].kind = FD_KIND_PIPE_W;
    t->fds[w].pipe = p;
    out[0] = r;
    out[1] = w;
    return 0;
}

int sched_pipe_new(void)
{
    return alloc_pipe();
}

void sched_pipe_unused(int pipe_id)
{
    if (pipe_id < 0 || pipe_id >= PIPE_MAX) {
        return;
    }
    if (pipes[pipe_id].used == 0) {
        return;
    }
    if (pipes[pipe_id].rrefs == 0 && pipes[pipe_id].wrefs == 0) {
        free_pipe(pipe_id);
    }
}

int sched_fd_bind_pipe(int fd, int kind, int pipe_id)
{
    struct task *t;
    struct fd_entry *e;

    if (last_added < 0 || last_added >= task_count) {
        return -1;
    }
    if (fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    if (kind != FD_KIND_PIPE_R && kind != FD_KIND_PIPE_W) {
        return -1;
    }
    if (pipe_id < 0 || pipe_id >= PIPE_MAX || pipes[pipe_id].used == 0) {
        return -1;
    }
    t = &tasks[last_added];
    if (t->state == TASK_DEAD) {
        return -1;
    }
    e = &t->fds[fd];
    if (e->used != 0) {
        return -1;
    }
    e->used = 1;
    e->kind = kind;
    e->pipe = pipe_id;
    e->path[0] = '\0';
    if (kind == FD_KIND_PIPE_R) {
        pipes[pipe_id].rrefs++;
    } else {
        pipes[pipe_id].wrefs++;
    }
    return 0;
}

int sched_pipe_read(int fd, void *dst, unsigned n)
{
    struct pipe *p;
    unsigned i;
    unsigned got;
    uint8_t *out;
    int idx;

    if (n == 0) {
        return 0;
    }
    if (dst == 0 || sched_fd_kind(fd) != FD_KIND_PIPE_R) {
        return -1;
    }
    idx = tasks[current].fds[fd].pipe;
    if (idx < 0 || idx >= PIPE_MAX || pipes[idx].used == 0 ||
        pipes[idx].data == 0) {
        return -1;
    }
    p = &pipes[idx];
    if (p->len == 0) {
        if (p->wrefs == 0) {
            return 0;
        }
        return -2;
    }
    got = n;
    if (got > p->len) {
        got = p->len;
    }
    out = (uint8_t *)dst;
    for (i = 0; i < got; i++) {
        out[i] = p->data[(p->head + i) & PIPE_MASK];
    }
    p->head = (p->head + got) & PIPE_MASK;
    p->len -= got;
    wake_pipe_waiters(idx);
    return (int)got;
}

int sched_pipe_write(int fd, const void *src, unsigned n)
{
    struct pipe *p;
    unsigned i;
    unsigned got;
    unsigned room;
    unsigned tail;
    const uint8_t *in;
    int idx;

    if (n == 0) {
        return 0;
    }
    if (src == 0 || sched_fd_kind(fd) != FD_KIND_PIPE_W) {
        return -1;
    }
    idx = tasks[current].fds[fd].pipe;
    if (idx < 0 || idx >= PIPE_MAX || pipes[idx].used == 0 ||
        pipes[idx].data == 0) {
        return -1;
    }
    p = &pipes[idx];
    if (p->rrefs == 0) {
        return -1;
    }
    if (p->len >= PIPE_CAP) {
        return -2;
    }
    room = PIPE_CAP - p->len;
    got = n;
    if (got > room) {
        got = room;
    }
    in = (const uint8_t *)src;
    tail = (p->head + p->len) & PIPE_MASK;
    for (i = 0; i < got; i++) {
        p->data[(tail + i) & PIPE_MASK] = in[i];
    }
    p->len += got;
    wake_pipe_waiters(idx);
    return (int)got;
}

void sched_block_pipe(struct interrupt_frame *frame, int pipe_id)
{
    int next;

    if (frame == 0 || task_count == 0) {
        return;
    }
    save_task(&tasks[current], frame);
    tasks[current].state = TASK_PIPE;
    tasks[current].pipe_wait = pipe_id;
    next = pick_next(current);
    if (next < 0) {
        idle_until_ready(frame);
        return;
    }
    set_current(next);
    load_task(frame, &tasks[current]);
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

/* Parent (waiter) row: "st N" once per distinct code. */
static void show_wait_status(struct task *t, uint8_t code)
{
    if (t == 0) {
        return;
    }
    if (t->seen_status != 0 && t->shown_status == code) {
        return;
    }
    t->seen_status = 1;
    t->shown_status = code;
    vga_write_at(t->row, 12, "st    ");
    vga_write_dec_at(t->row, 15, (unsigned)code);
}

static void wake_waiters(uint8_t code)
{
    int i;

    last_exit = code;
    have_exit = 1;
    for (i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_WAIT) {
            tasks[i].state = TASK_READY;
            tasks[i].rax = (uint64_t)code;
            show_wait_status(&tasks[i], code);
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
    int next;

    if (frame == 0) {
        return;
    }
    /* last_exit survives DEAD-slot reuse so a later waiter still sees the code. */
    if (have_exit != 0) {
        frame->rax = (uint64_t)last_exit;
        show_wait_status(&tasks[current], last_exit);
        return;
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

void sched_reset_current(struct interrupt_frame *frame, uint64_t rip, uint64_t rsp,
                         uint64_t user_base)
{
    struct task *t;

    if (frame == 0 || task_count == 0 || rip == 0 || rsp == 0 || user_base == 0) {
        return;
    }
    t = &tasks[current];
    if (t->state == TASK_DEAD || t->cr3 == 0 || t->kstack_top == 0) {
        return;
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
    t->user_base = user_base;
    t->state = TASK_READY;
    t->writes = 0;
    t->wake_tick = 0;
    t->pipe_wait = -1;
    clear_fds(t);
    load_task(frame, t);
}

void sched_exit(struct interrupt_frame *frame)
{
    int next;
    int row;
    uint8_t code;

    if (task_count != 0) {
        code = (uint8_t)(frame != 0 ? (frame->rdi & 0xffull) : 0);
        row = tasks[current].row;
        vga_write_at(row, 2, "done");
        /* Still on this task's kstack until iretq; free that on slot reuse. */
        free_task_user(&tasks[current]);
        clear_fds(&tasks[current]);
        tasks[current].exit_code = code;
        tasks[current].state = TASK_DEAD;
        wake_waiters(code);
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
