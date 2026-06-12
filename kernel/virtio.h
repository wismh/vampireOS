#pragma once

#include <stdint.h>

int virtio_init(int row);
int virtio_ready(void);
int virtio_read(uint32_t lba, unsigned sectors, void *dst);
