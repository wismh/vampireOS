#include "pmm.h"
#include "vga.h"

#include <stdint.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1u << PAGE_SHIFT)
/* Must match boot/const.inc and the 2 MiB identity map in stage 2. */
#define KERNEL_PHYS 0x100000ull
#define KERNEL_SIZE (32ull * 512ull)
#define IDENTITY_END 0x200000ull

static uint8_t *bitmap;
static uint64_t frame_count;
static uint64_t free_count;
static uint64_t reserved_end;

static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static uint64_t align_down(uint64_t value, uint64_t align)
{
    return value & ~(align - 1);
}

static void pmm_mark_used(uint64_t frame)
{
    uint64_t byte;
    uint8_t bit;

    if (frame >= frame_count) {
        return;
    }
    byte = frame >> 3;
    bit = (uint8_t)(1u << (frame & 7));
    if ((bitmap[byte] & bit) == 0) {
        bitmap[byte] |= bit;
        free_count--;
    }
}

static void pmm_mark_free(uint64_t frame)
{
    uint64_t byte;
    uint8_t bit;

    if (frame >= frame_count) {
        return;
    }
    byte = frame >> 3;
    bit = (uint8_t)(1u << (frame & 7));
    if ((bitmap[byte] & bit) != 0) {
        bitmap[byte] = (uint8_t)(bitmap[byte] & (uint8_t)~bit);
        free_count++;
    }
}

static void pmm_mark_region_free(uint64_t base, uint64_t length)
{
    uint64_t start;
    uint64_t end;

    if (length == 0) {
        return;
    }
    end = base + length;
    if (end < base) {
        return;
    }
    start = align_up(base, PAGE_SIZE);
    end = align_down(end, PAGE_SIZE);
    while (start < end) {
        pmm_mark_free(start >> PAGE_SHIFT);
        start += PAGE_SIZE;
    }
}

void pmm_init(const struct e820_map *map)
{
    uint32_t i;
    uint32_t n = 0;
    uint64_t max_phys = 0;
    uint64_t bitmap_phys;
    uint64_t bitmap_bytes;
    uint64_t room_frames;
    uint64_t addr;

    bitmap = 0;
    frame_count = 0;
    free_count = 0;
    reserved_end = 0;

    if (map != 0 && map->entry_size == E820_ENTRY_SIZE) {
        n = map->count;
        if (n > E820_MAX) {
            n = E820_MAX;
        }
    }

    for (i = 0; i < n; i++) {
        const struct e820_entry *e = &map->entries[i];
        uint64_t end;

        if (e->type != E820_TYPE_USABLE || e->length == 0) {
            continue;
        }
        end = e->base + e->length;
        if (end < e->base) {
            continue;
        }
        if (end > max_phys) {
            max_phys = end;
        }
    }

    bitmap_phys = align_up(KERNEL_PHYS + KERNEL_SIZE, PAGE_SIZE);
    if (bitmap_phys >= IDENTITY_END) {
        return;
    }

    room_frames = (IDENTITY_END - bitmap_phys) << 3;
    frame_count = max_phys >> PAGE_SHIFT;
    if (frame_count > room_frames) {
        frame_count = room_frames;
    }
    if (frame_count == 0) {
        return;
    }

    bitmap_bytes = (frame_count + 7) >> 3;
    if (bitmap_phys + bitmap_bytes > IDENTITY_END) {
        frame_count = 0;
        return;
    }

    bitmap = (uint8_t *)(uintptr_t)bitmap_phys;
    for (addr = 0; addr < bitmap_bytes; addr++) {
        bitmap[addr] = 0xFF;
    }

    for (i = 0; i < n; i++) {
        const struct e820_entry *e = &map->entries[i];

        if (e->type == E820_TYPE_USABLE) {
            pmm_mark_region_free(e->base, e->length);
        }
    }

    reserved_end = align_up(bitmap_phys + bitmap_bytes, PAGE_SIZE);
    addr = 0;
    while (addr < reserved_end) {
        pmm_mark_used(addr >> PAGE_SHIFT);
        addr += PAGE_SIZE;
    }
}

uint64_t pmm_alloc(void)
{
    uint64_t frame;

    if (bitmap == 0) {
        return 0;
    }

    for (frame = 0; frame < frame_count; frame++) {
        uint8_t bit = (uint8_t)(1u << (frame & 7));

        if ((bitmap[frame >> 3] & bit) == 0) {
            pmm_mark_used(frame);
            return frame << PAGE_SHIFT;
        }
    }
    return 0;
}

void pmm_free(uint64_t phys)
{
    if (phys < reserved_end || (phys & (PAGE_SIZE - 1)) != 0) {
        return;
    }
    pmm_mark_free(phys >> PAGE_SHIFT);
}

uint64_t pmm_free_count(void)
{
    return free_count;
}

int pmm_print(int row)
{
    uint64_t a;
    uint64_t b;
    uint64_t c;

    if (row >= VGA_HEIGHT - 2) {
        return row;
    }

    vga_write_at(row, 0, "pmm ");
    vga_write_dec_at(row, 4, (unsigned)free_count);
    row++;

    a = pmm_alloc();
    b = pmm_alloc();
    c = pmm_alloc();
    vga_write_hex64_at(row, 0, a);
    vga_write_hex64_at(row, 17, b);
    vga_write_hex64_at(row, 34, c);
    pmm_free(a);
    pmm_free(b);
    pmm_free(c);
    return row + 1;
}
