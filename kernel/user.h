#pragma once

#include <stdint.h>

int gdt_init(void);
uint64_t gdt_base(void);
int user_init(int row);
int user_ready(void);
__attribute__((noreturn)) void user_enter(void);
void user_on_syscall(uint64_t cs, uint64_t nr, uint64_t arg);
