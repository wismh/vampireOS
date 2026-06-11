#pragma once

#include <stdint.h>

int ahci_init(int row);
int ahci_ready(void);
int ahci_read(uint32_t lba, unsigned sectors, void *dst);
int ahci_write(uint32_t lba, unsigned sectors, const void *src);
