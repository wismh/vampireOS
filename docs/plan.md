# February 2026

Vampire OS after `vos-55`: BIOS MBR, FAT12 on ATA PIO (128 data clusters), private CR3 per task, ring-3 `open`/`read`/`write`/`cat`, shell `run cat <path>`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 64 KiB (`KERNEL_SECTORS` 128); bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again.

## Now

- Volume: 128 data clusters, files up to 4 KiB. Subdirs and the root grow across FAT chains.
- Shell: `help ls mem cat run put rm mv fill mkdir rmdir cd pwd`. Kernel `cat` reads the volume; `run cat <path>` uses the user ELF.
- Tasks: every ELF at `0x400000`, stack at `0x401000`. Per-task cloned PML4; switch loads `task->cr3`. Exit tears down user PTEs; PML4 freed on slot reuse. `TASK_MAX` 8.
- Syscalls: write (legacy string or fd), exit, yield, sleep, wait, open, close, read. Four fds per task. `run` loads any FAT12 ELF into a free slot.
- **Argv:** `run` pushes `argc` / `argv[]` / NULL on the user stack before start. `cat.asm` reads `argv[1]`.
- No pipes, exec, readdir, brk, fork, long names, second FAT sector, UEFI, AHCI.

## Week 1 — finish the root volume

Subdirs already chain; rename updates the parent entry. Root grows across FAT clusters like subdirs.

1. **`mv`** — rename a file or empty directory on FAT12 (update the directory entry name in the parent; refuse cross-directory moves for this slice). `mv note dusk` after `put note x` shows `dusk` in `ls`. `mv` onto an existing name prints `?`.
2. **Root directory chains** — like subdirs: when the 16 root slots fill, allocate the next cluster and extend the root FAT chain. `put` of a 17th root file still works. Subdir code paths stay unchanged.

## Week 2 — real arguments

Stop poking paths into a fixed user address before `run`.

3. **`argv` for `run`** — done: before start, push `argc` / `argv[]` / NULL on the user stack. `cat.asm` reads `argv[1]`. `user_run_path` / `USER_ARG_PATH` are gone. `run cat hello` unchanged at the shell.
4. **`readdir` + user `ls`** — syscall that fills a user buffer with names in the cwd (or open `.` semantics). Pack `user/ls.asm`; `run ls` lists the volume root or cwd from ring 3. Keep kernel `ls` working.

## Week 3 — replace the running program

`run` always spawns a new task today; the shell stays in the kernel.

5. **`exec`** — load an ELF over the **current** task: unmap old user mappings in its CR3, map the new file at `0x400000`, jump to entry. One slot, no extra task count. Prove with a tiny `user/exit.asm` that `exec`s into `echo` (or rename `run` → `exec` for one program first).
6. **Exit status** — `exit` takes an 8-bit code; `wait` returns it in `rax` (or a second syscall). Parent task row shows the code once. Needed before shell scripts and pipes.

## Week 4 — connect programs

7. **`pipe`** — syscall returns two fds (read end, write end); small kernel ring buffer (one page). `read`/`write` on those fds block or return partial data. No `fork` yet — prove with two tasks or a self-pipe in one ELF.
8. **Shell `|`** — kernel line parser runs left and right of `|` with a pipe between them, e.g. `cat hello | cat` (second cat copies to VGA via fd 1) or `run cat hello | run wc` once `wc` exists. Start with one hard-coded pair if a full parser is too big.

## Leave for later

UEFI, AHCI, FAT16, long names, **fork/COW**, a libc, a framebuffer, networking, SMP, **brk/mmap**, hard links, directories > cluster without chaining bugs, a second FAT sector when clusters exceed one sector’s map.

`fork` and a real heap (`brk`) come after exec, exit status, and pipes — not before.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
