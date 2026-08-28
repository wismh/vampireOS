---
tags: [architecture]
---

# Boundaries

Rules that keep slices small. Break one only with a `vos-N` that also updates this note.

## Boot vs kernel

- Stage 2 may program COM1, VBE, E820, and page tables. It must not parse FAT or create tasks.
- `kmain` must not assume identity-mapped low RAM after `vmm_drop_identity`. VGA, LFB, bitmap, and page tables go through the HHDM.

## FS vs block

- `kernel/fs.c` speaks LBAs relative to `bio_part_lba()`. It does not pick VirtIO vs AHCI vs ATA.
- A new block backend registers `read`/`write` (and optional `flush`) with `bdev_register`. It does not know 8.3 names.

## Tasks vs FS

- Scheduler owns fd slots, cwd cluster/path, and pipes. It calls `fs_*` by path; it does not walk FAT tables.
- `exec` / `run` load bytes with `fs_lookup` then `elf_load`. The loader must not mount volumes.

## Userland vs kernel

- User programs are freestanding. No `FILE*`, no hosted libc, no dynamic linker.
- Syscalls are `int 0x30` with numbers in `kernel/user.c`. CRT stubs live in `user/crt.asm` / `user/fd.asm`.
- Kernel pointers from ring 3 must fail; copies walk the task’s page tables.

## Console vs FB

- 80×25 text VGA is the fallback if VBE fails. Shell must still type `$`.
- `SYS_FBPIX` paints a PMM shadow; `SYS_FBPRESENT` blits. Games do not map the LFB themselves.

## Net

- virtio-net is TX-only at boot. No sockets, no RX queue, no ARP/DHCP/TCP in the kernel.

## Size

- Do not grow `KERNEL_SECTORS` without `PART_LBA`, the image concat, and `KERNEL_SIZE`.
- `TASK_MAX` is 16; eight fds per task. Raise them only as their own slices.
