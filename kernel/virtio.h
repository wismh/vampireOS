#pragma once

#include <stdint.h>

int virtio_init(int row);
int virtio_net_init(int row);
int virtio_ready(void);
int virtio_read(uint32_t lba, unsigned sectors, void *dst);
int virtio_write(uint32_t lba, unsigned sectors, const void *src);
