#pragma once

#include <stdint.h>

int bread(uint32_t lba, unsigned sectors, void *dst);
int bwrite(uint32_t lba, unsigned sectors, const void *src);
int bdev_register(const char *name,
    int (*read)(uint32_t lba, unsigned sectors, void *dst),
    int (*write)(uint32_t lba, unsigned sectors, const void *src));
/* Sector count of the device just registered (0 = unknown). */
void bdev_set_sectors(uint32_t sectors);
/* Cache flush of the device just registered. */
void bdev_set_flush(int (*flush)(void));
/* Flush the active FS device. 0 ok, -1 if none or the command fails. */
int bflush(void);
/* Pack probed names into dst (space-separated). Returns bytes, or -1. */
int bdev_list(char *dst, unsigned max);
/* Read MBR LBA 0 and set the FAT partition start. 0 ok, -1 no table. */
int bio_init(void);
uint32_t bio_part_lba(void);
