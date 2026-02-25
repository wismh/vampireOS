#pragma once

#include "idt.h"

#include <stdint.h>

int gdt_init(void);
uint64_t gdt_base(void);
void tss_set_rsp0(uint64_t rsp0);
int user_init(int row);
int user_ready(void);
/* Load ELF `name`; `arg` is argv[1] or 0. Argv sits on the user stack. */
int user_run(const char *name, const char *arg);
/* Spawn left with stdout on a pipe and right with stdin on that pipe. */
int user_run_pipeline(const char *left_name, const char *left_arg,
                      const char *right_name, const char *right_arg);
__attribute__((noreturn)) void user_enter(void);
void user_on_syscall(struct interrupt_frame *frame);
