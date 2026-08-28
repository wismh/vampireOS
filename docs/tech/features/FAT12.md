---
tags: [feature]
---

# FAT12

Volume after the kernel pad. BPB numbers live in `boot/const.inc` and the image packer.

## Layout

- Type `0x01` partition at LBA **273**.
- 1 reserved, 2 FATs × 4 sectors, 16 root entries, 1024 data clusters, 1 sector/cluster.
- Media `0xF8`. Dual FAT copies.

## Names and links

- 8.3 plus VFAT LFN (ASCII). `ls` / `open` / `cat longname.txt`; create via `put` / `cp` / `>`.
- `ln hello dual` — second dirent, same first cluster. Last `rm` frees the chain (nlink = scan of that directory).

## Mutations

- `mv` same parent or cross-directory (rewrite both dirents; clusters stay).
- `trunc name N` sets size and frees trailing clusters.
- `sync` writes both FATs + cwd, then device flush.

Proof: `$` `cat hello` prints `blood`. `$` `cat bad` (cluster `0x0F00` past the 1307-sector image) prints `?`.

See [[modules/FS]], [[features/Block backends]].
