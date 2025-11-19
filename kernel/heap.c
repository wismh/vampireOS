#include "heap.h"
#include "pmm.h"
#include "vga.h"

#include <stdint.h>

#define PAGE_SIZE 0x1000ull
#define HEAP_PAGES 2ull
#define HEAP_ALIGN 16ull
#define HEAP_MAGIC 0x48454150u
#define HEAP_MIN_PHYS 0x200000ull

struct block {
    uint32_t magic;
    uint32_t free;
    uint64_t size;
};

static uint8_t *heap_base;
static uint8_t *heap_end;

static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static struct block *next_block(struct block *b)
{
    uint8_t *p;

    p = (uint8_t *)(b + 1) + b->size;
    if (p < (uint8_t *)(b + 1) || p + sizeof(struct block) > heap_end) {
        return 0;
    }
    return (struct block *)p;
}

void kheap_init(void)
{
    uint64_t pages[2];
    uint64_t n = 0;
    uint64_t i;
    struct block *b;

    heap_base = 0;
    heap_end = 0;

    for (i = 0; i < HEAP_PAGES; i++) {
        pages[i] = pmm_alloc_above(HEAP_MIN_PHYS);
        if (pages[i] == 0) {
            break;
        }
        if (i > 0 && pages[i] != pages[0] + i * PAGE_SIZE) {
            pmm_free(pages[i]);
            break;
        }
        n++;
    }
    if (n == 0) {
        return;
    }

    heap_base = (uint8_t *)(uintptr_t)pages[0];
    heap_end = heap_base + n * PAGE_SIZE;
    b = (struct block *)heap_base;
    b->magic = HEAP_MAGIC;
    b->free = 1;
    b->size = (uint64_t)(heap_end - heap_base) - sizeof(struct block);
}

void *kmalloc(uint64_t size)
{
    struct block *b;
    uint64_t need;
    uint64_t leftover;

    if (heap_base == 0 || size == 0) {
        return 0;
    }

    need = align_up(size, HEAP_ALIGN);
    b = (struct block *)heap_base;
    while (b != 0) {
        if (b->magic != HEAP_MAGIC) {
            return 0;
        }
        if (b->free && b->size >= need) {
            leftover = b->size - need;
            if (leftover >= sizeof(struct block) + HEAP_ALIGN) {
                struct block *split = (struct block *)((uint8_t *)(b + 1) + need);

                split->magic = HEAP_MAGIC;
                split->free = 1;
                split->size = leftover - sizeof(struct block);
                b->size = need;
            }
            b->free = 0;
            return b + 1;
        }
        b = next_block(b);
    }
    return 0;
}

void kfree(void *ptr)
{
    struct block *b;
    struct block *n;
    uint8_t *p;

    if (ptr == 0 || heap_base == 0) {
        return;
    }
    p = (uint8_t *)ptr;
    if (p < heap_base + sizeof(struct block) || p >= heap_end) {
        return;
    }

    b = (struct block *)ptr - 1;
    if (b->magic != HEAP_MAGIC || b->free) {
        return;
    }
    b->free = 1;

    n = next_block(b);
    if (n != 0 && n->magic == HEAP_MAGIC && n->free) {
        b->size += sizeof(struct block) + n->size;
    }
}

int kheap_print(int row)
{
    void *a;
    void *b;
    void *c;

    if (row >= VGA_HEIGHT - 2) {
        return row;
    }

    a = kmalloc(16);
    b = kmalloc(16);
    kfree(a);
    c = kmalloc(16);

    vga_write_at(row, 0, "heap");
    vga_write_hex64_at(row, 5, (uint64_t)(uintptr_t)a);
    vga_write_hex64_at(row, 22, (uint64_t)(uintptr_t)b);
    vga_write_hex64_at(row, 39, (uint64_t)(uintptr_t)c);

    kfree(b);
    kfree(c);
    return row + 1;
}
