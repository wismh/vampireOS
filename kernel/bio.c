#include "bio.h"

#define BDEV_MAX 4

struct bdev {
    const char *name;
    int (*read)(uint32_t lba, unsigned sectors, void *dst);
    int (*write)(uint32_t lba, unsigned sectors, const void *src);
    int (*flush)(void);
    uint32_t sectors;
};

static struct bdev table[BDEV_MAX];
static unsigned n_devs;
static uint32_t part_lba;

static int past_end(uint32_t lba, unsigned sectors)
{
    uint32_t abs;
    uint32_t lim;

    if (sectors == 0 || n_devs == 0) {
        return 1;
    }
    abs = lba + part_lba;
    lim = table[0].sectors;
    if (lim == 0) {
        return 0;
    }
    if (abs < lba || abs >= lim || sectors > lim - abs) {
        return 1;
    }
    return 0;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int bio_init(void)
{
    uint8_t mbr[512];
    unsigned i;
    uint8_t type;
    uint32_t start;
    uint32_t count;

    part_lba = 0;
    if (n_devs == 0 || table[0].read == 0) {
        return -1;
    }
    if (table[0].read(0, 1u, mbr) != 0) {
        return -1;
    }
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        return -1;
    }
    for (i = 0; i < 4u; i++) {
        type = mbr[0x1BE + i * 16u + 4u];
        start = rd32(mbr + 0x1BE + i * 16u + 8u);
        count = rd32(mbr + 0x1BE + i * 16u + 12u);
        if (type == 0x01 && start != 0 && count != 0) {
            part_lba = start;
            return 0;
        }
    }
    return -1;
}

uint32_t bio_part_lba(void)
{
    return part_lba;
}

int bdev_register(const char *name,
    int (*read)(uint32_t lba, unsigned sectors, void *dst),
    int (*write)(uint32_t lba, unsigned sectors, const void *src))
{
    if (name == 0 || read == 0 || write == 0 || n_devs >= BDEV_MAX) {
        return -1;
    }
    table[n_devs].name = name;
    table[n_devs].read = read;
    table[n_devs].write = write;
    table[n_devs].flush = 0;
    table[n_devs].sectors = 0;
    n_devs++;
    return 0;
}

void bdev_set_sectors(uint32_t sectors)
{
    if (n_devs == 0) {
        return;
    }
    table[n_devs - 1u].sectors = sectors;
}

void bdev_set_flush(int (*flush)(void))
{
    if (n_devs == 0) {
        return;
    }
    table[n_devs - 1u].flush = flush;
}

int bflush(void)
{
    if (n_devs == 0 || table[0].flush == 0) {
        return -1;
    }
    return table[0].flush();
}

int bread(uint32_t lba, unsigned sectors, void *dst)
{
    if (n_devs == 0 || table[0].read == 0 || dst == 0 || past_end(lba, sectors)) {
        return -1;
    }
    return table[0].read(lba + part_lba, sectors, dst);
}

int bwrite(uint32_t lba, unsigned sectors, const void *src)
{
    if (n_devs == 0 || table[0].write == 0 || src == 0 || past_end(lba, sectors)) {
        return -1;
    }
    return table[0].write(lba + part_lba, sectors, src);
}

int bdev_list(char *dst, unsigned max)
{
    unsigned i;
    unsigned n = 0;
    const char *s;

    if (dst == 0 || max == 0) {
        return -1;
    }
    for (i = 0; i < n_devs; i++) {
        s = table[i].name;
        if (s == 0) {
            continue;
        }
        if (n > 0) {
            if (n + 1u >= max) {
                break;
            }
            dst[n++] = ' ';
        }
        while (*s != '\0' && n + 1u < max) {
            dst[n++] = *s;
            s++;
        }
    }
    dst[n] = '\0';
    return (int)n;
}
