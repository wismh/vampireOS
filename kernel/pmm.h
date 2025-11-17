#pragma once

#include "e820.h"

#include <stdint.h>

void pmm_init(const struct e820_map *map);
uint64_t pmm_alloc(void);
uint64_t pmm_alloc_above(uint64_t min_phys);
void pmm_free(uint64_t phys);
uint64_t pmm_free_count(void);
int pmm_print(int row);
