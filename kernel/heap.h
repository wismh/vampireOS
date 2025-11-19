#pragma once

#include <stdint.h>

void kheap_init(void);
void *kmalloc(uint64_t size);
void kfree(void *ptr);
int kheap_print(int row);
