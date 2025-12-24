#include "fs.h"
#include "ata.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

#define FS_MAX 8
#define SEC 512u
#define FILE_MAX 0x1000u
#define CHAIN_MAX 8u
#define FAT12_EOF 0xFF8u
#define FAT12_MAX 4084u

struct fs_file {
    char name[13];
    uint8_t *data;
    unsigned len;
    unsigned cluster;
    unsigned dir_off;
    unsigned is_dir;
};

static struct fs_file files[FS_MAX];
static int file_count;
static uint8_t *g_fat;
static uint8_t *g_sec;
static unsigned g_first_data;
static unsigned g_first_root;
static unsigned g_rsvd;
static unsigned g_fatsz;
static unsigned g_root_ent;
static unsigned g_clusters;
static unsigned g_cwd;
static uint8_t *g_dir;

static unsigned rd_u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned rd_u32(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

static void wr_u16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr_u32(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static int names_eq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int find_file(const char *name)
{
    int i;

    if (name == 0 || *name == '\0') {
        return -1;
    }
    for (i = 0; i < file_count; i++) {
        if (names_eq(name, files[i].name)) {
            return i;
        }
    }
    return -1;
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

static void drop_page(uint8_t *p)
{
    uint64_t phys;

    if (p == 0) {
        return;
    }
    phys = virt_to_phys((uint64_t)(uintptr_t)p);
    if (phys != 0) {
        pmm_free(phys);
    }
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

static void fat12_set(uint8_t *fat, unsigned cl, unsigned val)
{
    unsigned off = cl + cl / 2u;

    val &= 0xFFFu;
    if (off + 1u >= SEC) {
        return;
    }
    if ((cl & 1u) != 0) {
        fat[off] = (uint8_t)((fat[off] & 0x0Fu) | ((val & 0x0Fu) << 4));
        fat[off + 1u] = (uint8_t)(val >> 4);
    } else {
        fat[off] = (uint8_t)val;
        fat[off + 1u] = (uint8_t)((fat[off + 1u] & 0xF0u) | (val >> 8));
    }
}

static int name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static char fold_az(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int to_83(const char *name, uint8_t *out)
{
    unsigned i;
    unsigned n = 0;
    unsigned e = 0;

    for (i = 0; i < 11u; i++) {
        out[i] = ' ';
    }
    if (name == 0 || *name == '\0') {
        return -1;
    }
    while (name[n] != '\0' && name[n] != '.' && n < 8u) {
        char c = fold_az(name[n]);
        if (!name_char(c)) {
            return -1;
        }
        if (c >= 'a' && c <= 'z') {
            out[n] = (uint8_t)(c - 'a' + 'A');
        } else {
            out[n] = (uint8_t)c;
        }
        n++;
    }
    if (n == 0) {
        return -1;
    }
    if (name[n] == '.') {
        name += n + 1u;
        while (name[e] != '\0' && e < 3u) {
            char c = fold_az(name[e]);
            if (!name_char(c)) {
                return -1;
            }
            if (c >= 'a' && c <= 'z') {
                out[8u + e] = (uint8_t)(c - 'a' + 'A');
            } else {
                out[8u + e] = (uint8_t)c;
            }
            e++;
        }
        if (e == 0 || name[e] != '\0') {
            return -1;
        }
    } else if (name[n] != '\0') {
        return -1;
    }
    return 0;
}

static int fat_flush(void)
{
    if (g_fat == 0 || g_fatsz == 0) {
        return -1;
    }
    if (ata_write(g_rsvd, 1u, g_fat) != 0) {
        return -1;
    }
    return ata_write(g_rsvd + g_fatsz, 1u, g_fat);
}

static unsigned fat_alloc(void)
{
    unsigned cl;
    unsigned last = g_clusters + 1u;

    for (cl = 2u; cl <= last; cl++) {
        if (fat12_ent(g_fat, cl) == 0) {
            fat12_set(g_fat, cl, FAT12_EOF);
            return cl;
        }
    }
    return 0;
}

static unsigned dir_lba(void)
{
    if (g_cwd < 2u) {
        return g_first_root;
    }
    return g_first_data + (g_cwd - 2u);
}

static unsigned dir_ents(void)
{
    if (g_cwd < 2u) {
        return g_root_ent;
    }
    return SEC / 32u;
}

static int dir_load(void)
{
    if (g_sec == 0) {
        return -1;
    }
    return ata_read(dir_lba(), 1u, g_sec);
}

static int dir_store(void)
{
    if (g_sec == 0) {
        return -1;
    }
    return ata_write(dir_lba(), 1u, g_sec);
}

static int dir_slot(void)
{
    unsigned i;
    unsigned n = dir_ents();

    if (dir_load() != 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        uint8_t c = g_sec[i * 32u];
        if (c == 0 || c == 0xE5) {
            return (int)(i * 32u);
        }
    }
    return -1;
}

static void fat_free_chain(unsigned cl)
{
    unsigned hops = 0;

    while (cl >= 2u && cl < FAT12_EOF && hops < CHAIN_MAX) {
        unsigned next = fat12_ent(g_fat, cl);
        fat12_set(g_fat, cl, 0);
        cl = next;
        hops++;
    }
}

static int fat_resize(unsigned *start, unsigned need)
{
    unsigned cls[CHAIN_MAX];
    unsigned have = 0;
    unsigned cl;
    unsigned n;

    if (start == 0 || need == 0 || need > CHAIN_MAX) {
        return -1;
    }
    cl = *start;
    while (cl >= 2u && cl < FAT12_EOF && have < CHAIN_MAX) {
        cls[have++] = cl;
        cl = fat12_ent(g_fat, cl);
    }
    while (have < need) {
        n = fat_alloc();
        if (n < 2u) {
            return -1;
        }
        if (have > 0) {
            fat12_set(g_fat, cls[have - 1u], n);
        } else {
            *start = n;
        }
        cls[have++] = n;
    }
    while (have > need) {
        have--;
        fat12_set(g_fat, cls[have], 0);
    }
    fat12_set(g_fat, cls[need - 1u], FAT12_EOF);
    *start = cls[0];
    return 0;
}

static int store_chain(unsigned start, const uint8_t *src, unsigned len)
{
    unsigned cl = start;
    unsigned off = 0;
    unsigned hops = 0;
    unsigned k;

    while (off < len) {
        unsigned n = len - off;

        if (n > SEC) {
            n = SEC;
        }
        if (cl < 2u || cl >= FAT12_EOF || hops >= CHAIN_MAX || g_sec == 0) {
            return -1;
        }
        for (k = 0; k < SEC; k++) {
            g_sec[k] = 0;
        }
        copy_bytes(g_sec, src + off, n);
        if (ata_write(g_first_data + (cl - 2u), 1u, g_sec) != 0) {
            return -1;
        }
        off += n;
        hops++;
        if (off < len) {
            cl = fat12_ent(g_fat, cl);
        }
    }
    return 0;
}

static void fat_name(const uint8_t *ent, char *out);

static int fs_create(const char *name)
{
    uint8_t n83[11];
    unsigned cl;
    int off;
    unsigned k;
    uint8_t *ent;
    uint8_t *dst;

    if (to_83(name, n83) != 0 || file_count >= FS_MAX || g_fat == 0) {
        return -1;
    }
    dst = page_buf();
    if (dst == 0) {
        return -1;
    }
    cl = fat_alloc();
    if (cl < 2u) {
        drop_page(dst);
        return -1;
    }
    if (fat_flush() != 0) {
        fat12_set(g_fat, cl, 0);
        drop_page(dst);
        return -1;
    }
    off = dir_slot();
    if (off < 0) {
        fat12_set(g_fat, cl, 0);
        (void)fat_flush();
        drop_page(dst);
        return -1;
    }
    ent = g_sec + (unsigned)off;
    for (k = 0; k < 32u; k++) {
        ent[k] = 0;
    }
    copy_bytes(ent, n83, 11u);
    ent[11] = 0x20;
    wr_u16(ent + 26, cl);
    if (dir_store() != 0) {
        fat12_set(g_fat, cl, 0);
        (void)fat_flush();
        drop_page(dst);
        return -1;
    }
    fat_name(ent, files[file_count].name);
    files[file_count].data = dst;
    files[file_count].len = 0;
    files[file_count].cluster = cl;
    files[file_count].dir_off = (unsigned)off;
    files[file_count].is_dir = 0;
    file_count++;
    return file_count - 1;
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

static int load_file(uint8_t *dst, unsigned start, unsigned size)
{
    unsigned left = size;
    unsigned cl = start;
    unsigned hops = 0;

    while (left > 0) {
        unsigned n;

        if (cl < 2u || cl >= FAT12_EOF || hops >= CHAIN_MAX || g_fat == 0 || g_sec == 0) {
            return -1;
        }
        if (ata_read(g_first_data + (cl - 2u), 1u, g_sec) != 0) {
            return -1;
        }
        n = left < SEC ? left : SEC;
        copy_bytes(dst, g_sec, n);
        dst += n;
        left -= n;
        cl = fat12_ent(g_fat, cl);
        hops++;
    }
    return 0;
}

static int scan_dir(void)
{
    unsigned i;
    unsigned n;
    int old = file_count;

    for (i = 0; i < (unsigned)old; i++) {
        drop_page(files[i].data);
        files[i].data = 0;
    }
    file_count = 0;
    if (g_dir == 0 || ata_read(dir_lba(), 1u, g_dir) != 0) {
        return -1;
    }
    n = dir_ents();
    for (i = 0; i < n && file_count < FS_MAX; i++) {
        const uint8_t *ent = g_dir + i * 32u;
        unsigned attr;
        unsigned cl;
        unsigned sz;
        uint8_t *dst;

        if (ent[0] == 0) {
            break;
        }
        if (ent[0] == 0xE5 || ent[0] == 0x05 || ent[0] == '.') {
            continue;
        }
        attr = ent[11];
        if ((attr & 0x08u) != 0 || attr == 0x0Fu) {
            continue;
        }
        cl = rd_u16(ent + 26);
        fat_name(ent, files[file_count].name);
        files[file_count].cluster = cl;
        files[file_count].dir_off = i * 32u;
        files[file_count].data = 0;
        files[file_count].len = 0;
        if ((attr & 0x10u) != 0) {
            files[file_count].is_dir = 1;
            file_count++;
            continue;
        }
        sz = rd_u32(ent + 28);
        if (sz == 0 || sz > FILE_MAX) {
            continue;
        }
        dst = page_buf();
        if (dst == 0) {
            return -1;
        }
        if (load_file(dst, cl, sz) != 0) {
            drop_page(dst);
            return -1;
        }
        files[file_count].data = dst;
        files[file_count].len = sz;
        files[file_count].is_dir = 0;
        file_count++;
    }
    return 0;
}

int fs_init(int row)
{
    uint8_t *scratch;
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

    file_count = 0;
    g_fat = 0;
    g_sec = 0;
    g_dir = 0;
    g_fatsz = 0;
    g_cwd = 0;
    scratch = page_buf();
    if (scratch == 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
    }
    g_fat = scratch;
    g_sec = scratch + SEC;
    g_dir = scratch + SEC * 2u;
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
    g_rsvd = rsvd;
    g_fatsz = fatsz;
    g_root_ent = root_ent;
    g_clusters = clusters;
    g_first_root = rsvd + nfats * fatsz;
    g_first_data = g_first_root + root_secs;
    g_cwd = 0;
    if (ata_read(rsvd, 1u, g_fat) != 0 || scan_dir() != 0) {
        vga_write_at(row, 0, "fat fail");
        return row + 1;
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

    if (data == 0 || len == 0) {
        return -1;
    }
    i = find_file(name);
    if (i < 0 || files[i].is_dir || g_sec == 0 || files[i].data == 0) {
        return -1;
    }
    if (dir_load() != 0) {
        return -1;
    }
    files[i].len = rd_u32(g_sec + files[i].dir_off + 28);
    if (files[i].len == 0 || files[i].len > FILE_MAX) {
        return -1;
    }
    if (load_file(files[i].data, files[i].cluster, files[i].len) != 0) {
        return -1;
    }
    *data = files[i].data;
    *len = files[i].len;
    return 0;
}

int fs_write(const char *name, const void *src, unsigned len)
{
    int i;
    unsigned need;

    i = find_file(name);
    if (i >= 0 && files[i].is_dir) {
        return -1;
    }
    if (i < 0) {
        i = fs_create(name);
    }
    if (i < 0 || src == 0 || len == 0 || len > FILE_MAX || g_sec == 0 ||
        files[i].is_dir) {
        return -1;
    }
    need = (len + SEC - 1u) / SEC;
    if (fat_resize(&files[i].cluster, need) != 0 || fat_flush() != 0) {
        return -1;
    }
    if (store_chain(files[i].cluster, (const uint8_t *)src, len) != 0) {
        return -1;
    }
    if (dir_load() != 0) {
        return -1;
    }
    wr_u32(g_sec + files[i].dir_off + 28, len);
    wr_u16(g_sec + files[i].dir_off + 26, files[i].cluster);
    if (dir_store() != 0) {
        return -1;
    }
    copy_bytes(files[i].data, (const uint8_t *)src, len);
    files[i].len = len;
    return 0;
}

int fs_remove(const char *name)
{
    int i;
    uint64_t phys;

    i = find_file(name);
    if (i < 0 || files[i].is_dir || g_sec == 0 || g_fat == 0) {
        return -1;
    }
    fat_free_chain(files[i].cluster);
    if (fat_flush() != 0) {
        return -1;
    }
    if (dir_load() != 0) {
        return -1;
    }
    g_sec[files[i].dir_off] = 0xE5;
    if (dir_store() != 0) {
        return -1;
    }
    phys = virt_to_phys((uint64_t)(uintptr_t)files[i].data);
    if (i != file_count - 1) {
        files[i] = files[file_count - 1];
    }
    file_count--;
    if (phys != 0) {
        pmm_free(phys);
    }
    return 0;
}

int fs_isdir(int i)
{
    if (i < 0 || i >= file_count) {
        return 0;
    }
    return files[i].is_dir ? 1 : 0;
}

int fs_mkdir(const char *name)
{
    uint8_t n83[11];
    unsigned cl;
    unsigned k;
    unsigned parent;
    int off;
    uint8_t *ent;

    if (to_83(name, n83) != 0 || find_file(name) >= 0 || file_count >= FS_MAX ||
        g_fat == 0 || g_sec == 0) {
        return -1;
    }
    cl = fat_alloc();
    if (cl < 2u) {
        return -1;
    }
    if (fat_flush() != 0) {
        fat12_set(g_fat, cl, 0);
        return -1;
    }
    parent = g_cwd < 2u ? 0 : g_cwd;
    for (k = 0; k < SEC; k++) {
        g_sec[k] = 0;
    }
    g_sec[0] = '.';
    for (k = 1; k < 11u; k++) {
        g_sec[k] = ' ';
    }
    g_sec[11] = 0x10;
    wr_u16(g_sec + 26, cl);
    g_sec[32] = '.';
    g_sec[33] = '.';
    for (k = 34; k < 43u; k++) {
        g_sec[k] = ' ';
    }
    g_sec[43] = 0x10;
    wr_u16(g_sec + 58, parent);
    if (ata_write(g_first_data + (cl - 2u), 1u, g_sec) != 0) {
        fat12_set(g_fat, cl, 0);
        (void)fat_flush();
        return -1;
    }
    off = dir_slot();
    if (off < 0) {
        fat12_set(g_fat, cl, 0);
        (void)fat_flush();
        return -1;
    }
    ent = g_sec + (unsigned)off;
    for (k = 0; k < 32u; k++) {
        ent[k] = 0;
    }
    copy_bytes(ent, n83, 11u);
    ent[11] = 0x10;
    wr_u16(ent + 26, cl);
    if (dir_store() != 0) {
        fat12_set(g_fat, cl, 0);
        (void)fat_flush();
        return -1;
    }
    return scan_dir();
}

int fs_chdir(const char *name)
{
    int i;

    if (name == 0 || *name == '\0') {
        return -1;
    }
    if (names_eq(name, ".")) {
        return 0;
    }
    if (names_eq(name, "..")) {
        if (g_cwd < 2u) {
            return 0;
        }
        if (ata_read(dir_lba(), 1u, g_sec) != 0) {
            return -1;
        }
        g_cwd = rd_u16(g_sec + 58);
        return scan_dir();
    }
    i = find_file(name);
    if (i < 0 || files[i].is_dir == 0) {
        return -1;
    }
    g_cwd = files[i].cluster;
    return scan_dir();
}
