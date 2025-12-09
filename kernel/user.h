#pragma once

#include "idt.h"

#include <stdint.h>

int gdt_init(void);
uint64_t gdt_base(void);
void tss_set_rsp0(uint64_t rsp0);
int user_init(int row);
int user_ready(void);
__attribute__((noreturn)) void user_enter(void);
void user_on_syscall(struct interrupt_frame *frame);
