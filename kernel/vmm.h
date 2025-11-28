#pragma once

#include "e820.h"

#include <stdint.h>

#define HHDM_BASE 0xFFFF800000000000ull

void vmm_map_usable(const struct e820_map *map);
void vmm_hhdm_init(void);
int vmm_map_user(uint64_t virt, uint64_t phys);
int vmm_drop_identity(int row);
__attribute__((noreturn)) void vmm_switch_stack(void (*cont)(void));
uint64_t phys_to_virt(uint64_t phys);
uint64_t virt_to_phys(uint64_t virt);
int vmm_print(int row);
