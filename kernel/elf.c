#include "elf.h"

#include <stdint.h>

#define ELF_MAGIC0 0x7F
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_EV_CURRENT 1
#define ELF_ET_EXEC 2
#define ELF_EM_X86_64 62
#define ELF_PT_LOAD 1
#define ELF_LOAD_MAX 8

struct elf64_ehdr {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

_Static_assert(sizeof(struct elf64_ehdr) == 64, "ELF64 Ehdr is 64 bytes");
_Static_assert(sizeof(struct elf64_phdr) == 56, "ELF64 Phdr is 56 bytes");

static int elf_parse(const void *file, unsigned len, const struct elf64_ehdr **eh_out,
                     const struct elf64_phdr **phs, unsigned *nph)
{
    const uint8_t *raw = (const uint8_t *)file;
    const struct elf64_ehdr *eh;
    unsigned i;
    unsigned n;
    unsigned hit;
    uint64_t off;

    if (file == 0 || eh_out == 0 || phs == 0 || nph == 0 || len < sizeof(*eh)) {
        return -1;
    }
    eh = (const struct elf64_ehdr *)raw;
    if (eh->ident[0] != ELF_MAGIC0 || eh->ident[1] != 'E' || eh->ident[2] != 'L' ||
        eh->ident[3] != 'F' || eh->ident[4] != ELF_CLASS64 ||
        eh->ident[5] != ELF_DATA2LSB || eh->ident[6] != ELF_EV_CURRENT) {
        return -1;
    }
    if (eh->type != ELF_ET_EXEC || eh->machine != ELF_EM_X86_64 || eh->ehsize != 64 ||
        eh->phentsize != 56 || eh->phnum == 0) {
        return -1;
    }
    if (eh->phoff > len || eh->phnum > 8u) {
        return -1;
    }
    off = eh->phoff + (uint64_t)eh->phnum * 56ull;
    if (off > (uint64_t)len) {
        return -1;
    }

    n = 0;
    for (i = 0; i < eh->phnum; i++) {
        const struct elf64_phdr *p =
            (const struct elf64_phdr *)(raw + eh->phoff + (uint64_t)i * 56ull);

        if (p->type != ELF_PT_LOAD || p->memsz == 0) {
            continue;
        }
        if (n >= ELF_LOAD_MAX) {
            return -1;
        }
        if (p->filesz > p->memsz || p->offset > (uint64_t)len) {
            return -1;
        }
        if (p->filesz > (uint64_t)len - p->offset) {
            return -1;
        }
        if (p->vaddr + p->memsz < p->vaddr) {
            return -1;
        }
        phs[n] = p;
        n++;
    }
    if (n == 0) {
        return -1;
    }

    for (i = 1; i < n; i++) {
        const struct elf64_phdr *key = phs[i];
        unsigned j = i;

        while (j > 0 && phs[j - 1]->vaddr > key->vaddr) {
            phs[j] = phs[j - 1];
            j--;
        }
        phs[j] = key;
    }
    for (i = 1; i < n; i++) {
        if (phs[i]->vaddr < phs[i - 1]->vaddr + phs[i - 1]->memsz) {
            return -1;
        }
    }

    hit = 0;
    for (i = 0; i < n; i++) {
        if (eh->entry >= phs[i]->vaddr && eh->entry < phs[i]->vaddr + phs[i]->memsz) {
            hit = 1;
        }
    }
    if (hit == 0) {
        return -1;
    }

    *eh_out = eh;
    *nph = n;
    return 0;
}

int elf_image_base(const void *file, unsigned len, uint64_t *base)
{
    const struct elf64_ehdr *eh;
    const struct elf64_phdr *phs[ELF_LOAD_MAX];
    unsigned n;

    if (base == 0 || elf_parse(file, len, &eh, phs, &n) != 0) {
        return -1;
    }
    (void)eh;
    *base = phs[0]->vaddr;
    return 0;
}

int elf_load(const void *file, unsigned len, void *dest, unsigned dest_size,
             uint64_t vaddr, uint64_t *entry)
{
    const uint8_t *raw = (const uint8_t *)file;
    const struct elf64_ehdr *eh;
    const struct elf64_phdr *phs[ELF_LOAD_MAX];
    unsigned n;
    unsigned s;
    unsigned i;
    uint8_t *out;
    uint64_t rel;

    if (dest == 0 || entry == 0 || elf_parse(file, len, &eh, phs, &n) != 0) {
        return -1;
    }
    if (phs[0]->vaddr != vaddr) {
        return -1;
    }

    out = (uint8_t *)dest;
    for (i = 0; i < dest_size; i++) {
        out[i] = 0;
    }
    for (s = 0; s < n; s++) {
        const struct elf64_phdr *ph = phs[s];

        rel = ph->vaddr - vaddr;
        if (rel > (uint64_t)dest_size || ph->memsz > (uint64_t)dest_size - rel) {
            return -1;
        }
        for (i = 0; i < (unsigned)ph->filesz; i++) {
            out[rel + i] = raw[ph->offset + i];
        }
        /* p_filesz < p_memsz is BSS; dest was wiped, zero the tail anyway. */
        for (i = (unsigned)ph->filesz; i < (unsigned)ph->memsz; i++) {
            out[rel + i] = 0;
        }
    }
    *entry = eh->entry;
    return 0;
}
