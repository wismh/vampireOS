#pragma once

#include <stdint.h>

#define E820_MAX 32
#define E820_ENTRY_SIZE 24
#define E820_TYPE_USABLE 1

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed));

struct e820_map {
    uint32_t count;
    uint32_t entry_size;
    struct e820_entry entries[E820_MAX];
} __attribute__((packed));

int e820_print(const struct e820_map *map);
