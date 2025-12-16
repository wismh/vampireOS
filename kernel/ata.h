#pragma once

#include <stdint.h>

int ata_read(uint32_t lba, unsigned sectors, void *dst);
