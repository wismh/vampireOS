#pragma once

#include <stdint.h>

int bread(uint32_t lba, unsigned sectors, void *dst);
int bwrite(uint32_t lba, unsigned sectors, const void *src);
int bdev_register(const char *name,
    int (*read)(uint32_t lba, unsigned sectors, void *dst),
    int (*write)(uint32_t lba, unsigned sectors, const void *src));
/* Pack probed names into dst (space-separated). Returns bytes, or -1. */
int bdev_list(char *dst, unsigned max);
