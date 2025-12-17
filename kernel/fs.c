#include "fs.h"
#include "ata.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

#define FS_MAX 8
#define SEC 512u
#define FAT12_EOF 0xFF8u
#define FAT12_MAX 4084u

struct fs_file {
    char name[13];
    const void *data;
    unsigned len;
};

static struct fs_file files[FS_MAX];
static int file_count;

static unsigned rd_u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned rd_u32(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static uint8_t *page_buf(void)
{
    uint64_t phys;

    phys = pmm_alloc_above(0x200000ull);
    if (phys == 0) {
        phys = pmm_alloc();
    }
    if (phys == 0) {
        return 0;
    }
    return (uint8_t *)(uintptr_t)phys_to_virt(phys);
}

static unsigned fat12_ent(const uint8_t *fat, unsigned cl)
{
    unsigned off = cl + cl / 2u;
    unsigned v;

    if (off + 1u >= SEC) {
        return FAT12_EOF;
    }
    v = (unsigned)fat[off] | ((unsigned)fat[off + 1u] << 8);
    if ((cl & 1u) != 0) {
        v >>= 4;
    } else {
        v &= 0xFFFu;
    }
    return v;
}

static void fat_name(const uint8_t *ent, char *out)
{
    int i;
    int n = 0;
    char c;

    for (i = 0; i < 8; i++) {
        c = (char)ent[i];
        if (c == ' ') {
            break;
        }
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        out[n++] = c;
    }
    if (ent[8] != ' ') {
        out[n++] = '.';
        for (i = 8; i < 11; i++) {
            c = (char)ent[i];
            if (c == ' ') {
                break;
            }
            if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 'a');
            }
            out[n++] = c;
        }
    }
    out[n] = '\0';
}

static int load_file(const uint8_t *fat, uint8_t *sec, uint8_t *dst,
                     unsigned first_data, unsigned start, unsigned size)
{
    unsigned left = size;
    unsigned cl = start;
    unsigned hops = 0;

    while (left > 0) {
        unsigned n;

        if (cl < 2u || cl >= FAT12_EOF || hops >= 8u) {
            return -1;
        }
        if (ata_read(first_data + (cl - 2u), 1u, sec) != 0) {
            return -1;
        }
        n = left < SEC ? left : SEC;
        copy_bytes(dst, sec, n);
        dst += n;
        left -= n;
        cl = fat12_ent(fat, cl);
        hops++;
    }
    return 0;
}

int fs_init(int row)
{
    uint8_t *scratch;
    uint8_t *fat;
    uint8_t *root;
    uint8_t *sec;
    uint8_t *data;
    uint8_t *bpb;
    unsigned byts;
    unsigned spc;
    unsigned rsvd;
    unsigned nfats;
    unsigned root_ent;
    unsigned tot16;
    unsigned tot32;
    unsigned fatsz;
    unsigned tot;
    unsigned root_secs;
    unsigned data_secs;
    unsigned clusters;
    unsigned first_root;
    unsigned first_data;
    unsigned used = 0;
    unsigned i;

    file_count = 0;
    scratch = page_buf();
    data = page_buf();
    if (scratch == 0 || data == 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    fat = scratch;
    root = scratch + SEC;
    sec = scratch + SEC * 2u;
    bpb = scratch + SEC * 3u;

    if (ata_read(0u, 1u, bpb) != 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    byts = rd_u16(bpb + 11);
    spc = bpb[13];
    rsvd = rd_u16(bpb + 14);
    nfats = bpb[16];
    root_ent = rd_u16(bpb + 17);
    tot16 = rd_u16(bpb + 19);
    fatsz = rd_u16(bpb + 22);
    tot32 = rd_u32(bpb + 32);
    tot = tot16 != 0 ? tot16 : tot32;
    if (byts != SEC || spc != 1u || rsvd == 0 || nfats == 0 || fatsz != 1u ||
        root_ent == 0 || tot == 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    root_secs = ((root_ent * 32u) + (SEC - 1u)) / SEC;
    if (tot < rsvd + nfats * fatsz + root_secs) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    data_secs = tot - rsvd - nfats * fatsz - root_secs;
    clusters = data_secs / spc;
    if (clusters < 2u || clusters > FAT12_MAX) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    first_root = rsvd + nfats * fatsz;
    first_data = first_root + root_secs;
    if (ata_read(rsvd, 1u, fat) != 0 || ata_read(first_root, 1u, root) != 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }

    for (i = 0; i < root_ent && file_count < FS_MAX; i++) {
        const uint8_t *ent = root + i * 32u;
        unsigned attr;
        unsigned cl;
        unsigned sz;

        if (ent[0] == 0) {
            break;
        }
        if (ent[0] == 0xE5 || ent[0] == 0x05) {
            continue;
        }
        attr = ent[11];
        if ((attr & 0x08u) != 0 || (attr & 0x10u) != 0 || attr == 0x0Fu) {
            continue;
        }
        cl = rd_u16(ent + 26);
        sz = rd_u32(ent + 28);
        if (sz == 0 || sz > SEC * 8u || used + sz > 0x1000u) {
            continue;
        }
        if (load_file(fat, sec, data + used, first_data, cl, sz) != 0) {
            vga_write_at(row, 0, "fat fail");
            return row + 1;
        }
        fat_name(ent, files[file_count].name);
        files[file_count].data = data + used;
        files[file_count].len = sz;
        used += sz;
        file_count++;
    }
    if (file_count == 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    vga_write_at(row, 0, "fat 12");
    return row + 1;
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
