# FS

FAT12 volume in the MBR partition. 8.3 + VFAT LFN, hard links, cwd.

## Capabilities

- 1024 data clusters, four FAT sectors, two FAT copies, 16 root entries then cluster growth.
- `open`/`read`/`write`/`lseek`/`stat`/`readdir`; `put`/`cp`/`rm`/`mv`/`mkdir`/`rmdir`/`ln`/`trunc`.
- `sync` rewrites both FATs and the cwd directory, then `bflush`.
- Hard links: second dirent, same first cluster; last `rm` frees the chain.
- Lookup is relative to the calling task’s cwd (kernel shell has its own).

## How it is implemented

- [kernel/fs.c](../files/kernel.fs.c.md) — BPB, FAT walk, LFN read/create, dirents.
- Partition start from [kernel/bio.c](../files/kernel.bio.c.md) (`bio_part_lba()`, printed `part 273`).
- Packed `hello` / `motd` / `longname.txt` / `bad` come from the image packer ([Disk image](../build/disk-image.md)).

Does not own PCI. Block errors: `bread` `-1` → `cat bad` prints `?`.

## See also

- [FAT12](../features/fat12.md)
- [Block](block.md)
- [kernel/fs.h](../files/kernel.fs.h.md)
