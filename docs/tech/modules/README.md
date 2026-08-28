# Modules

Each area of the tree: what it can do and where the code lives. [Home](../README.md).

| Module | Owns |
| --- | --- |
| [Boot](boot.md) | MBR, stage 2, E820, VBE, long mode |
| [Memory](memory.md) | PMM, VMM, HHDM, kernel heap, ELF load |
| [Tasks](tasks.md) | GDT/TSS, `int 0x30`, scheduler, fds, signals |
| [FS](fs.md) | FAT12, LFN, dirents, cwd |
| [Block](block.md) | `bread` / `bwrite` / `bflush`, VirtIO-blk / AHCI / ATA |
| [Console](console.md) | VGA, COM1, PS/2, VBE overlay |
| [Net](net.md) | virtio-net probe + one TX UDP |
| [Userland](userland.md) | CRT, `init` / `sh`, tests |
