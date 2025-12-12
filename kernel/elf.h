#pragma once

#include <stdint.h>

int elf_load(const void *file, unsigned len, void *dest, unsigned dest_size,
             uint64_t vaddr, uint64_t *entry);
