#include "bio.h"

#define BDEV_MAX 4

struct bdev {
    const char *name;
    int (*read)(uint32_t lba, unsigned sectors, void *dst);
    int (*write)(uint32_t lba, unsigned sectors, const void *src);
};

static struct bdev table[BDEV_MAX];
static unsigned n_devs;

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
    n_devs++;
    return 0;
}

int bread(uint32_t lba, unsigned sectors, void *dst)
{
    if (n_devs == 0 || table[0].read == 0) {
        return -1;
    }
    return table[0].read(lba, sectors, dst);
}

int bwrite(uint32_t lba, unsigned sectors, const void *src)
{
    if (n_devs == 0 || table[0].write == 0) {
        return -1;
    }
    return table[0].write(lba, sectors, src);
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
