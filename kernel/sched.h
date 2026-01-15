#pragma once

#include "idt.h"

#include <stdint.h>

void sched_init(void);
int sched_add_user(uint64_t rip, uint64_t rsp, uint64_t kstack_top, int row,
                   uint64_t user_base);
int sched_base_busy(uint64_t user_base);
uint64_t sched_current_cr3(void);
int sched_row(void);
unsigned sched_note_write(void);
void sched_on_tick(struct interrupt_frame *frame);
void sched_yield(struct interrupt_frame *frame);
void sched_sleep(struct interrupt_frame *frame, uint64_t ticks);
void sched_wait(struct interrupt_frame *frame);
void sched_exit(struct interrupt_frame *frame);
