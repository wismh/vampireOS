#include "sched.h"
#include "vga.h"

#include <stdint.h>

#define TASK_MAX 2
#define TASK_DEAD 0
#define TASK_READY 1
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

static void sched_switch(struct interrupt_frame *frame)
{
    int next;

    if (frame == 0 || task_count < 2) {
        return;
    }
    next = current ^ 1;
    if (next >= task_count || tasks[next].state != TASK_READY) {
        return;
    }
    save_task(&tasks[current], frame);
    current = next;
    load_task(frame, &tasks[current]);
}

void sched_on_tick(struct interrupt_frame *frame)
{
    if (frame == 0 || (frame->cs & 3ull) != 3ull) {
        return;
    }
    sched_switch(frame);
}

void sched_exit(struct interrupt_frame *frame)
{
    int next;

    if (task_count != 0) {
        tasks[current].state = TASK_DEAD;
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
