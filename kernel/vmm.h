#pragma once

#include "e820.h"

void vmm_map_usable(const struct e820_map *map);
int vmm_print(int row);
