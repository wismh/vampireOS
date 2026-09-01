# Block

Device table under FAT. Active volume: VirtIO-blk → AHCI → ATA.

## Capabilities

- `bread` / `bwrite` / `bflush` / `bdev_list` (`$` `devs` → `virt ahci ata`).
- MBR parse: type `0x01` start LBA (`PART_LBA` 273).
- Write barrier: VirtIO `BLK_T_FLUSH` (`virt flush`), else AHCI `E7h` (`ahci flush`), else ATA `E7h`.

## How it is implemented

- [kernel/bio.c](../files/kernel.bio.c.md) — table, partition, dispatch.
- [kernel/virtio.c](../files/kernel.virtio.c.md) — virtio-blk (`0x1AF4` / `0x1001` or `0x1042`); first write `virt wr`; also virtio-net (see [Net](net.md)).
- [kernel/ahci.c](../files/kernel.ahci.c.md) — class `0x0106`; `ahci 55aa`; first write `ahci wr`.
- [kernel/ata.c](../files/kernel.ata.c.md) — PIO fallback.

QEMU attaches the **same** `vampire.img` to IDE (BIOS boot), AHCI, and virtio-blk with `file.locking=off`, `cache=writethrough`, no `snapshot=on`.

## See also

- [Block backends](../features/block-backends.md)
- [Boundaries](../architecture/boundaries.md)
