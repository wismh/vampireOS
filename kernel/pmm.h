#pragma once

#include "e820.h"

#include <stdint.h>

void pmm_init(const struct e820_map *map);
uint64_t pmm_alloc(void);
uint64_t pmm_alloc_above(uint64_t min_phys);
/* n consecutive frames at/above 2 MiB. phys of the first, or 0. */
uint64_t pmm_alloc_span(uint64_t pages);
void pmm_free(uint64_t phys);
uint64_t pmm_free_count(void);
int pmm_print(int row);
