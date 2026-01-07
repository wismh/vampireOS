# January 2026

Vampire OS after `vos-37`: BIOS MBR, 80-sector kernel, FAT12 on ATA PIO, `mkdir` / `cd` / paths, four ring-3 tasks on one user map, ELF `run echo`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 40 KiB; bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again.

## Now

- Volume: 128 data clusters, 16 root entries, files up to 4 KiB. Subdirs grow across FAT clusters; root stays one sector.
- Shell: `help ls mem cat run put rm fill mkdir rmdir cd pwd`. Paths work.
- Tasks A/B/C are bytes poked into pages at `0x400000` / `0x402000` / `0x404000`. `echo` is an ELF at `0x406000`. `copy_from_user` trusts `[0x400000, 0x408000)`. `TASK_MAX` is 4.
- Syscalls: write, exit, yield, sleep, wait. No open/read. No per-task CR3.

## Week 1 — finish the volume

Directories exist and subdirs can span a FAT chain; root is still one sector. Next is remembering the path string.

1. **`rmdir`** — refuse unless the cluster holds only `.` / `..` (and `0x00` / `0xE5`). Free the cluster, mark the parent entry deleted. `rmdir sub` after `mkdir sub` leaves `ls` without `sub/`. `rmdir` of a file or a non-empty dir prints `?`.
2. **More data clusters** — raise `FAT_DATA_CLUSTERS` (and `FAT_TOTAL_SECS`) so nested dirs plus `fill` chains still fit. Keep FAT12 (clusters ≤ 4084) and one sector per FAT until a later slice needs two.
3. **Directory chains** — `dir_load` / `dir_store` / `dir_slot` / `scan_dir` walk a FAT chain instead of one LBA. `mkdir` of a 17th entry in a subdir allocates the next cluster. Root can stay one sector until this works for subdirs.
4. **`pwd`** — print the cwd as `/` or `/sub`. Needed once nested dirs are routine; store parent names while walking `..` or keep a small path buffer on `chdir`.

## Week 2 — programs from disk

Stop emitting machine code in `user.c`. The volume already knows how to store an ELF.

5. **ELF A, B, C** — `user/a.asm`, `b.asm`, `c.asm` packed next to `echo` on FAT12. `user_init` loads them with `elf_load` the same way `run echo` does. Drop `emit_*`. Boot screen still shows A / B / C counts.
6. **Same load address** — map every ELF at `0x400000` plus a stack page, not a hand-placed slot per task. Requires week 3 if two programs run at once; until then keep distinct vaddrs only if CR3 is still shared.
7. **`run` more than echo** — `run a` after A has exited (or never started) loads that file. `echo_running` becomes a free task slot, not a special case for one name.
8. **`TASK_MAX` 8** — enough for three boot tasks plus a few `run`s. Exit must free the code/stack pages or the PMM will drain across `run` loops.

## Week 3 — private address spaces

Shared user PTEs are why A/B/C/echo sit at different vaddrs and why `copy_from_user` is a range check.

9. **Clone kernel tables** — each task gets its own PML4. Copy kernel / HHDM entries from the boot tables; user entries start empty.
10. **CR3 on switch** — `sched` stores `cr3` in `struct task`. `load_task` / first `iretq` writes CR3. Idle / syscall entry still uses the kernel map.
11. **Map into the current CR3** — `vmm_map_user` takes a PML4 (or uses the running task). Boot tasks and `run` map code/stack only there. Two ELFs may both live at `0x400000`.
12. **User copy walks PTEs** — `copy_from_user` translates `rdi` through the current task’s tables, then copies. A range check against `USER_LIMIT` goes away. `write` of a kernel pointer from ring 3 fails.

## Week 4 — files from ring 3

The shell already reads the volume. User code still cannot.

13. **FDs** — syscall `open` (path in user memory) / `close`. A small per-task table (4 fds). `open` uses the same path walk as `fs_lookup`.
14. **`read` / `write` on fds** — `read` copies file bytes into a user buffer. `write` on fd 1 stays VGA; `write` on a file fd calls `fs_write`. Keep sizes inside `FILE_MAX`.
15. **User `cat`** — `user/cat.asm` `open` / `read` / `write` / `exit`. `run cat` with the path still passed from the shell for this month (argv is a follow-up if it does not fit).
16. **Unmap on exit** — drop the task’s user PTEs and free those frames; keep the kernel half of the cloned PML4 until the task slot is reused. After a few `run cat` loops, `mem` must not only fall.

## Leave for later

UEFI, AHCI, FAT16, long names, pipes, fork/COW, a libc, a framebuffer, networking, SMP. `mv`, argv/`exec`, and a real `brk` come after fds and CR3, not before.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
