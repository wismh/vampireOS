# Memory

Physical frames, higher-half map, kernel heap, and ELF load into user pages.

## Capabilities

- Bitmap PMM from usable E820 (`pmm_alloc` / `pmm_free` / spans).
- 2 MiB pages plus 4 KiB tails; HHDM for VGA, LFB, bitmap, page tables.
- Small kernel heap after identity drop.
- Load ELF `PT_LOAD` (including BSS and extra segments) into a task’s CR3.

## How it is implemented

- [kernel/pmm.c](../files/kernel.pmm.c.md) — `KERNEL_SIZE` 304 sectors; bitmap sits above that.
- [kernel/vmm.c](../files/kernel.vmm.c.md) — map usable, HHDM, drop identity, stack switch.
- [kernel/heap.c](../files/kernel.heap.c.md) — `kheap_init`.
- [kernel/e820.c](../files/kernel.e820.c.md) — print / walk the map stage 2 left at `0x4000`.
- [kernel/elf.c](../files/kernel.elf.c.md) — `elf_load` / `elf_image_base`; honors `p_filesz < p_memsz`.
- [kernel/linker.ld](../files/kernel.linker.ld.md) — kernel VMA `0xFFFFFFFF80000000 + 0x100000`.

User heap is **not** this module: `brk` / `mmap` live in [Tasks](tasks.md).

## See also

- [ELF and COW](../features/elf-and-cow.md)
- [Boot path](../architecture/boot-path.md)
- [Boundaries](../architecture/boundaries.md)
