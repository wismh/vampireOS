#pragma once

#include "e820.h"

#include <stdint.h>

void pmm_init(const struct e820_map *map);
uint64_t pmm_alloc(void);
void pmm_free(uint64_t phys);
uint64_t pmm_free_count(void);
int pmm_print(int row);
