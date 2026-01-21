#pragma once

#include "idt.h"

#include <stdint.h>

#define FD_MAX 4
#define FD_PATH_MAX 32

void sched_init(void);
int sched_add_user(uint64_t rip, uint64_t rsp, uint64_t kstack_top, int row,
                   uint64_t user_base, uint64_t cr3);
/* True if this CR3 already has a live mapping at user_base. */
int sched_base_busy(uint64_t user_base, uint64_t cr3);
uint64_t sched_current_cr3(void);
int sched_row(void);
unsigned sched_note_write(void);
/* Lowest free fd in the current task; stores path leaf/path for later read. */
int sched_fd_open(const char *path);
int sched_fd_close(int fd);
void sched_on_tick(struct interrupt_frame *frame);
void sched_yield(struct interrupt_frame *frame);
void sched_sleep(struct interrupt_frame *frame, uint64_t ticks);
void sched_wait(struct interrupt_frame *frame);
void sched_exit(struct interrupt_frame *frame);
