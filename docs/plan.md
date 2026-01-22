# January 2026

Vampire OS after `vos-37`: BIOS MBR, 80-sector kernel, FAT12 on ATA PIO, `mkdir` / `cd` / paths, four ring-3 tasks on one user map, ELF `run echo`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 56 KiB; bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again.

## Now

- Volume: 128 data clusters, 16 root entries, files up to 4 KiB. Subdirs grow across FAT clusters; root stays one sector.
- Shell: `help ls mem cat run put rm fill mkdir rmdir cd pwd`. Paths work.
- Every user ELF links at `0x400000` with stack at `0x401000` (top `0x402000`). Each task maps those pages privately into its own cloned PML4; concurrent A/B/C/echo share the same vaddr safely. `copy_from_user` / `copy_to_user` walk the current task’s PML4 (present+user PTEs) and copy through HHDM — a kernel VA or unmapped address from ring 3 fails (`user fail`). `TASK_MAX` is 8; exit frees that task’s user code/stack frames and its kernel stack so `run` loops do not drain the PMM.
- `run <name>` loads any FAT12 ELF linked at `0x400000` into a fresh task CR3 (same address is not a global collision). After C exits, `run c` reuses a free slot.
- Syscalls: write, exit, yield, sleep, wait, open, close, read. Per-task table of 4 fds; `open` copies the path via PTE walk and resolves with `fs_lookup`; `close` frees the slot. `read` copies file bytes into a user buffer (≤ `FILE_MAX`). `write` with `rdi` as a user string stays VGA; `rdi < 4` is fd-based (`rsi`/`rdx`, fd 1 = VGA, other open fds call `fs_write`). `run readtest` opens `hello`, reads, prints `blood`. Kernel pad 112 sectors.
- Context switch loads each task’s cloned PML4 (`task->cr3`); idle and syscall entry use the boot/kernel CR3. `vmm_map_user` / `vmm_unmap_user` take that PML4 phys and install private user PTEs (no boot→clone share).

## Week 1 — finish the volume

Directories exist and subdirs can span a FAT chain; root is still one sector. Next is remembering the path string.

1. **`rmdir`** — refuse unless the cluster holds only `.` / `..` (and `0x00` / `0xE5`). Free the cluster, mark the parent entry deleted. `rmdir sub` after `mkdir sub` leaves `ls` without `sub/`. `rmdir` of a file or a non-empty dir prints `?`.
2. **More data clusters** — raise `FAT_DATA_CLUSTERS` (and `FAT_TOTAL_SECS`) so nested dirs plus `fill` chains still fit. Keep FAT12 (clusters ≤ 4084) and one sector per FAT until a later slice needs two.
3. **Directory chains** — `dir_load` / `dir_store` / `dir_slot` / `scan_dir` walk a FAT chain instead of one LBA. `mkdir` of a 17th entry in a subdir allocates the next cluster. Root can stay one sector until this works for subdirs.
4. **`pwd`** — print the cwd as `/` or `/sub`. Needed once nested dirs are routine; store parent names while walking `..` or keep a small path buffer on `chdir`.

## Week 2 — programs from disk

Stop emitting machine code in `user.c`. The volume already knows how to store an ELF.

5. **ELF A, B, C** — `user/a.asm`, `b.asm`, `c.asm` packed next to `echo` on FAT12. `user_init` loads them with `elf_load` the same way `run echo` does. Drop `emit_*`. Boot screen still shows A / B / C counts.
6. **Same load address** — done with item 11: every ELF at `0x400000` plus a stack page; concurrent tasks rely on private CR3.
7. **`run` more than echo** — done: `run <name>` loads any FAT12 ELF at its linked BASE when that slot is free; exited tasks free their slot/base. `TASK_MAX` still 4.
8. **`TASK_MAX` 8** — done: eight task slots; exit (and DEAD-slot reuse) frees user code/stack pages and the kernel stack so sequential `run` loops do not drain free frames.

## Week 3 — private address spaces

Shared user PTEs are why A/B/C/echo sat at different vaddrs and why `copy_from_user` was a range check.

9. **Clone kernel tables** — done: each task gets its own PML4 via `vmm_clone_pml4`; kernel/HHDM half copied from boot, user half empty; `task->cr3` stores the phys addr. Boot CR3 still used for execution until item 10.
10. **CR3 on switch** — done: `load_task` / first `iretq` write `task->cr3`; idle and syscall entry use `vmm_boot_cr3()`. Until item 11, `vmm_share_user` copies boot user PML4 slots into each clone so mappings stay visible.
11. **Map into the current CR3** — done: `vmm_map_user` / `vmm_unmap_user` take a PML4 phys; boot tasks and `run` map code/stack only there. All ELFs link at `0x400000`.
12. **User copy walks PTEs** — done: `copy_from_user` translates `rdi` through `sched_current_cr3()` (present+user PTEs) and copies via HHDM. No `[USER_CODE, USER_LIMIT)` gate; `write` of a kernel pointer from ring 3 fails.

## Week 4 — files from ring 3

The shell already reads the volume. User code still cannot.

13. **FDs** — done: syscall `open` (path in user memory) / `close`. A small per-task table (4 fds). `open` uses the same path walk as `fs_lookup`. `run opentest` proves open/close on `hello`.
14. **`read` / `write` on fds** — done: `SYS_READ` copies file bytes into a user buffer; `write` keeps legacy VGA strings and treats `rdi < 4` as fd write (`fs_write` or console). `run readtest` prints `blood` from `hello`.
15. **User `cat`** — `user/cat.asm` `open` / `read` / `write` / `exit`. `run cat` with the path still passed from the shell for this month (argv is a follow-up if it does not fit).
16. **Unmap on exit** — drop the task’s user PTEs and free those frames; keep the kernel half of the cloned PML4 until the task slot is reused. After a few `run cat` loops, `mem` must not only fall.

## Leave for later

UEFI, AHCI, FAT16, long names, pipes, fork/COW, a libc, a framebuffer, networking, SMP. `mv`, argv/`exec`, and a real `brk` come after fds and CR3, not before.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
