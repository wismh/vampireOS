# Block backends

FAT `bread`/`bwrite` go through [kernel/bio.c](../files/kernel.bio.c.md). Probe order for the **active** volume: VirtIO-blk, else AHCI, else ATA PIO.

| Backend | Probe | Happy path print | Flush |
| --- | --- | --- | --- |
| virtio-blk | `0x1AF4` + `0x1001`/`0x1042` | `virt 55aa`, first write `virt wr` | `BLK_T_FLUSH` → `virt flush` |
| AHCI | class `0x0106` | `ahci 55aa`, first write `ahci wr` | `E7h` → `ahci flush` |
| ATA | PIO | (always registered) | `E7h` |

BIOS still boots **IDE**. QEMU must attach the same file three ways (`file.locking=off`, `cache=writethrough`, no snapshot on AHCI/VirtIO). `$` `devs` lists `virt ahci ata`. After `$ hi > virt.txt` then `$ sync`, a cold restart still `cat`s `virt.txt`.

See [Block](../modules/block.md), [CMake](../build/cmake.md) (run flags).
