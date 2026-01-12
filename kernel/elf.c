#include "elf.h"

#include <stdint.h>

#define ELF_MAGIC0 0x7F
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_EV_CURRENT 1
#define ELF_ET_EXEC 2
#define ELF_EM_X86_64 62
#define ELF_PT_LOAD 1

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
                     const struct elf64_phdr **ph_out)
{
    const uint8_t *raw = (const uint8_t *)file;
    const struct elf64_ehdr *eh;
    const struct elf64_phdr *ph;
    unsigned i;
    uint64_t off;

    if (file == 0 || len < sizeof(*eh)) {
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

    ph = 0;
    for (i = 0; i < eh->phnum; i++) {
        const struct elf64_phdr *p =
            (const struct elf64_phdr *)(raw + eh->phoff + (uint64_t)i * 56ull);

        if (p->type != ELF_PT_LOAD) {
            continue;
        }
        if (ph != 0) {
            return -1;
        }
        ph = p;
    }
    if (ph == 0 || ph->memsz == 0 || ph->filesz > ph->memsz || ph->offset > len ||
        ph->offset + ph->filesz > (uint64_t)len) {
        return -1;
    }
    if (eh->entry < ph->vaddr || eh->entry >= ph->vaddr + ph->memsz) {
        return -1;
    }
    *eh_out = eh;
    *ph_out = ph;
    return 0;
}

int elf_image_base(const void *file, unsigned len, uint64_t *base)
{
    const struct elf64_ehdr *eh;
    const struct elf64_phdr *ph;

    if (base == 0 || elf_parse(file, len, &eh, &ph) != 0) {
        return -1;
    }
    (void)eh;
    *base = ph->vaddr;
    return 0;
}

int elf_load(const void *file, unsigned len, void *dest, unsigned dest_size,
             uint64_t vaddr, uint64_t *entry)
{
    const uint8_t *raw = (const uint8_t *)file;
    const struct elf64_ehdr *eh;
    const struct elf64_phdr *ph;
    unsigned i;
    uint8_t *out;

    if (dest == 0 || entry == 0 || elf_parse(file, len, &eh, &ph) != 0) {
        return -1;
    }
    if (ph->vaddr != vaddr || ph->memsz > dest_size) {
        return -1;
    }

    out = (uint8_t *)dest;
    for (i = 0; i < dest_size; i++) {
        out[i] = 0;
    }
    for (i = 0; i < (unsigned)ph->filesz; i++) {
        out[i] = raw[ph->offset + i];
    }
    *entry = eh->entry;
    return 0;
}
