#include "sched.h"
#include "idt.h"
#include "vga.h"

#include <stdint.h>

#define TASK_MAX 3
#define TASK_DEAD 0
#define TASK_READY 1
#define TASK_SLEEP 2
#define TASK_WAIT 3
#define USER_CS 0x2B
#define USER_DS 0x23

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
    int state;
    int row;
    unsigned writes;
    unsigned wake_tick;
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

void sched_init(void)
{
    int i;

    task_count = 0;
    current = 0;
    for (i = 0; i < TASK_MAX; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].writes = 0;
        tasks[i].wake_tick = 0;
    }
}

int sched_add_user(uint64_t rip, uint64_t rsp, int row)
{
    struct task *t;

    if (task_count >= TASK_MAX) {
        return -1;
    }
    t = &tasks[task_count];
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
    t->state = TASK_READY;
    t->row = row;
    t->writes = 0;
    t->wake_tick = 0;
    if (task_count == 0) {
        current = 0;
    }
    task_count++;
    return 0;
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

    for (;;) {
        wake_sleepers();
        next = pick_next(current);
        if (next >= 0) {
            current = next;
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
    current = next;
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
    current = next;
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
    current = next;
    load_task(frame, &tasks[current]);
}

void sched_exit(struct interrupt_frame *frame)
{
    int next;
    int row;

    if (task_count != 0) {
        row = tasks[current].row;
        vga_write_at(row, 2, "done");
        tasks[current].state = TASK_DEAD;
        wake_waiters();
    }
    next = pick_next(current);
    if (next < 0 || frame == 0) {
        __asm__ volatile ("sti");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
    current = next;
    load_task(frame, &tasks[current]);
}
