#pragma once

#include <stdint.h>

int ata_read(uint32_t lba, unsigned sectors, void *dst);
int ata_write(uint32_t lba, unsigned sectors, const void *src);
