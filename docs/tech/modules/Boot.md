---
tags: [module]
---

# Boot

BIOS MBR + stage 2: real mode load, COM1, E820, VBE, long mode, jump to `kmain`.

## Capabilities

- Load 16 sectors of stage 2 and 256 sectors of kernel ([[build/Disk Image]]).
- Print `boot` on COM1 before VBE ([[features/Framebuffer]] still comes later in stage 2).
- Hand `kmain` an E820 map and optional linear framebuffer info.

## How it is implemented

- [[boot.boot.asm]] — MBR, partition table type `0x01` at `PART_LBA`.
- [[boot.stage2.asm]] — DAP reads, 16550, E820, VBE, paging, jump.
- [[boot.const.inc]] — `KERNEL_SECTORS`, `PART_LBA`, `KERNEL_VMA`, FAT BPB numbers used by the image packer.

Stage 2 identity-maps low RAM so the kernel can run at `KERNEL_VIRT` (`KERNEL_VMA + 0x100000`) with a matching physical load.

## See also

- [[architecture/Boot Path]]
- [[modules/Memory]]
- [[build/CMake]]
