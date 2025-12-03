#pragma once

#include "idt.h"

#include <stdint.h>

void sched_init(void);
int sched_add_user(uint64_t rip, uint64_t rsp, int row);
int sched_row(void);
unsigned sched_note_write(void);
void sched_on_tick(struct interrupt_frame *frame);
void sched_exit(struct interrupt_frame *frame);
