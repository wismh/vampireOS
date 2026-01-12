#pragma once

#include <stdint.h>

int elf_image_base(const void *file, unsigned len, uint64_t *base);
int elf_load(const void *file, unsigned len, void *dest, unsigned dest_size,
             uint64_t vaddr, uint64_t *entry);
