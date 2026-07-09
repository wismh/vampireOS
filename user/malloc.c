/* Tiny malloc/free on brk. Query with rdi=0; grow to a new break. */
void *brk(void *addr);

#define HEAP_ALIGN 16ul
#define HEAP_MAGIC 0x4d414c43u
#define HEAP_GROW 0x1000ul

struct block {
    unsigned magic;
    unsigned free;
    unsigned long size;
};

static unsigned char *heap_base;
static unsigned char *heap_end;

static unsigned long align_up(unsigned long value, unsigned long align)
{
    return (value + align - 1ul) & ~(align - 1ul);
}

static int brk_fail(void *p)
{
    return p == (void *)(unsigned long)-1ul;
}

static struct block *next_block(struct block *b)
{
    unsigned char *p;

    p = (unsigned char *)(b + 1) + b->size;
    if (p < (unsigned char *)(b + 1) || p + sizeof(struct block) > heap_end) {
        return 0;
    }
    return (struct block *)p;
}

static int grow(unsigned long need)
{
    unsigned char *cur;
    unsigned char *want;
    unsigned char *got;
    struct block *b;
    struct block *n;
    unsigned long extra;

    extra = need;
    if (extra < HEAP_GROW) {
        extra = HEAP_GROW;
    }
    extra = align_up(extra, HEAP_GROW);

    if (heap_base == 0) {
        cur = (unsigned char *)brk(0);
        if (cur == 0 || brk_fail(cur)) {
            return -1;
        }
        heap_base = cur;
        heap_end = cur;
    }

    want = heap_end + extra;
    got = (unsigned char *)brk(want);
    if (brk_fail(got) || got < want) {
        return -1;
    }

    extra = (unsigned long)(got - heap_end);
    if (heap_end == heap_base) {
        b = (struct block *)heap_base;
        b->magic = HEAP_MAGIC;
        b->free = 1;
        b->size = extra - sizeof(struct block);
        heap_end = got;
        return 0;
    }

    b = (struct block *)heap_base;
    n = next_block(b);
    while (n != 0) {
        b = n;
        n = next_block(b);
    }
    if (b->free) {
        b->size += extra;
    } else {
        n = (struct block *)heap_end;
        n->magic = HEAP_MAGIC;
        n->free = 1;
        n->size = extra - sizeof(struct block);
    }
    heap_end = got;
    return 0;
}

void *malloc(unsigned long size)
{
    struct block *b;
    unsigned long need;
    unsigned long leftover;

    if (size == 0) {
        return 0;
    }

    need = align_up(size, HEAP_ALIGN);
    if (heap_base == 0) {
        if (grow(sizeof(struct block) + need) != 0) {
            return 0;
        }
    }

    for (;;) {
        b = (struct block *)heap_base;
        while (b != 0) {
            if (b->magic != HEAP_MAGIC) {
                return 0;
            }
            if (b->free && b->size >= need) {
                leftover = b->size - need;
                if (leftover >= sizeof(struct block) + HEAP_ALIGN) {
                    struct block *split =
                        (struct block *)((unsigned char *)(b + 1) + need);

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
        if (grow(sizeof(struct block) + need) != 0) {
            return 0;
        }
    }
}

void free(void *ptr)
{
    struct block *b;
    struct block *n;
    unsigned char *p;

    if (ptr == 0 || heap_base == 0) {
        return;
    }
    p = (unsigned char *)ptr;
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
