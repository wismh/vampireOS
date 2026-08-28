---
tags: [module]
---

# FS

FAT12 volume in the MBR partition. 8.3 + VFAT LFN, hard links, cwd.

## Capabilities

- 1024 data clusters, four FAT sectors, two FAT copies, 16 root entries then cluster growth.
- `open`/`read`/`write`/`lseek`/`stat`/`readdir`; `put`/`cp`/`rm`/`mv`/`mkdir`/`rmdir`/`ln`/`trunc`.
- `sync` rewrites both FATs and the cwd directory, then `bflush`.
- Hard links: second dirent, same first cluster; last `rm` frees the chain.
- Lookup is relative to the calling task’s cwd (kernel shell has its own).

## How it is implemented

- [[kernel.fs.c]] — BPB, FAT walk, LFN read/create, dirents.
- Partition start from [[kernel.bio.c]] (`bio_part_lba()`, printed `part 273`).
- Packed `hello` / `motd` / `longname.txt` / `bad` come from the image packer ([[build/Disk Image]]).

Does not own PCI. Block errors: `bread` `-1` → `cat bad` prints `?`.

## See also

- [[features/FAT12]]
- [[modules/Block]]
- [[kernel.fs.h]]
