#include "fs.h"
#include "vmm.h"

#include <stdint.h>

#define FS_MAX 3
#define INITRD_PHYS 0x20000ull
#define INITRD_SIZE (8ull * 512ull)

struct fs_file {
    const char *name;
    const void *data;
    unsigned len;
};

static struct fs_file files[FS_MAX];
static int file_count;

static unsigned rd_u32(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

void fs_init(void)
{
    const uint8_t *base;
    const uint8_t *p;
    const uint8_t *end;
    unsigned n;
    unsigned i;

    file_count = 0;
    base = (const uint8_t *)(uintptr_t)phys_to_virt(INITRD_PHYS);
    end = base + INITRD_SIZE;
    if (base[0] != 'V' || base[1] != 'R' || base[2] != 'D' || base[3] != '1') {
        return;
    }
    n = rd_u32(base + 4);
    if (n == 0 || n > FS_MAX) {
        return;
    }
    p = base + 8;
    for (i = 0; i < n; i++) {
        unsigned nlen;
        unsigned dlen;

        if (p >= end) {
            file_count = 0;
            return;
        }
        nlen = p[0];
        p++;
        if (nlen == 0 || nlen > 15 || p + nlen + 1 + 4 > end) {
            file_count = 0;
            return;
        }
        files[i].name = (const char *)p;
        p += nlen;
        if (*p != 0) {
            file_count = 0;
            return;
        }
        p++;
        dlen = rd_u32(p);
        p += 4;
        if (p + dlen > end) {
            file_count = 0;
            return;
        }
        files[i].data = p;
        files[i].len = dlen;
        p += dlen;
        file_count++;
    }
}

int fs_count(void)
{
    return file_count;
}

const char *fs_name(int i)
{
    if (i < 0 || i >= file_count) {
        return 0;
    }
    return files[i].name;
}

int fs_lookup(const char *name, const void **data, unsigned *len)
{
    int i;
    const char *a;
    const char *b;

    if (name == 0 || data == 0 || len == 0) {
        return -1;
    }
    for (i = 0; i < file_count; i++) {
        a = name;
        b = files[i].name;
        while (*a != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            *data = files[i].data;
            *len = files[i].len;
            return 0;
        }
    }
    return -1;
}
