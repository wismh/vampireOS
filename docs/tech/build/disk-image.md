# Disk image

Concatenation, not a sparse filesystem image tool:

1. MBR `boot.bin` (512 B) with partition type `0x01` start `PART_LBA`.
2. Stage 2 (16 sectors).
3. Kernel pad (256 sectors). `kernel.raw.bin` is ~130724 of 131072 bytes (tight).
4. FAT12 volume (`FAT_TOTAL_SECS` from `boot/const.inc`) with packed ELFs and `hello` (`blood`).

Total **1307** sectors. `PART_LBA` = `1 + 16 + 256` = **273**. PMM still reserves **304** kernel sectors of physical RAM so virtio-net / ELF `.text` does not push `.bss` onto the bitmap.

When `kernel.raw.bin` no longer fits 256 sectors, bump `KERNEL_SECTORS`, `PART_LBA`, the concat, and `KERNEL_SIZE` in one slice.

See [boot/const.inc](../files/boot.const.inc.md), [Boot](../modules/boot.md), [FS](../modules/fs.md).
