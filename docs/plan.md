# February 2026

Vampire OS after `vos-55`: BIOS MBR, FAT12 on ATA PIO (128 data clusters), private CR3 per task, ring-3 `open`/`read`/`write`/`cat`, shell `run cat <path>`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 80 KiB (`KERNEL_SECTORS` 160); bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again.

## Now

- Volume: 128 data clusters, files up to 4 KiB. Subdirs and the root grow across FAT chains.
- Shell: `help ls mem cat run put rm mv fill mkdir rmdir cd pwd |`. Kernel `cat` reads the volume; `run cat <path>` uses the user ELF; `run ls` lists the cwd from ring 3. A line with `|` spawns left and right with a pipe between them.
- Tasks: every ELF at `0x400000`, stack at `0x401000`. Per-task cloned PML4; switch loads `task->cr3`. Exit tears down user PTEs; PML4 freed on slot reuse. `TASK_MAX` 8.
- Syscalls: write (legacy string or fd), exit (8-bit code in `rdi`), yield, sleep, wait (that code in `rax`), open, close, read, readdir, exec, pipe (`rdi` = user `int fd[2]`; fd[0] read, fd[1] write; `rax` 0 or -1). Four fds per task. `run` loads any FAT12 ELF into a free slot.
- **Argv:** `run` pushes `argc` / `argv[]` / NULL on the user stack before start. `cat.asm` reads `argv[1]`. `exec` does the same for the new image.
- **readdir:** syscall copies cwd names into a user buffer; `user/ls.asm` writes them. Kernel `ls` still works.
- **exec:** syscall loads an ELF over the current task (same slot, same CR3/kstack). `run exectest` becomes `echo` without growing the task count.
- **Exit status:** `exit` takes an 8-bit code; `wait` returns it in `rax`. The waiter’s VGA row shows `st N` once per distinct code.
- **pipe:** syscall 11 writes two fds into user `int fd[2]` via `rdi`. One-page kernel ring. `read`/`write` block when empty/full or return partial. `run pipetest` self-pipes `pipe` onto VGA.
- **Shell `|`:** one `|` splits the line; left runs with fd 1 on the write end, right with fd 0 on the read end. `cat hello | cat` and `run cat hello | run cat` copy `hello` through the ring onto VGA. Nested pipes not supported.
- No brk, fork, long names, second FAT sector, UEFI, AHCI.

## Week 1 — finish the root volume

Subdirs already chain; rename updates the parent entry. Root grows across FAT clusters like subdirs.

1. **`mv`** — rename a file or empty directory on FAT12 (update the directory entry name in the parent; refuse cross-directory moves for this slice). `mv note dusk` after `put note x` shows `dusk` in `ls`. `mv` onto an existing name prints `?`.
2. **Root directory chains** — like subdirs: when the 16 root slots fill, allocate the next cluster and extend the root FAT chain. `put` of a 17th root file still works. Subdir code paths stay unchanged.

## Week 2 — real arguments

Stop poking paths into a fixed user address before `run`.

3. **`argv` for `run`** — done: before start, push `argc` / `argv[]` / NULL on the user stack. `cat.asm` reads `argv[1]`. `user_run_path` / `USER_ARG_PATH` are gone. `run cat hello` unchanged at the shell.
4. **`readdir` + user `ls`** — done: syscall fills a user buffer with cwd names; `user/ls.asm` writes them. `run ls` lists the volume root or cwd from ring 3. Kernel `ls` still works.

## Week 3 — replace the running program

`run` always spawns a new task today; the shell stays in the kernel.

5. **`exec`** — done: load an ELF over the **current** task: unmap old user mappings in its CR3, map the new file at `0x400000`, jump to entry. One slot, no extra task count. `user/exectest.asm` execs into `echo`.
6. **Exit status** — done: `exit` takes an 8-bit code from `rdi`; `wait` returns it in `rax`. The waiter’s row shows `st N` once. `user/status.asm` exits 42; `user/waiter.asm` writes that code.

## Week 4 — connect programs

7. **`pipe`** — done: syscall 11 writes read/write fds into user `int fd[2]` via `rdi` (`rax` 0/-1). One-page kernel ring. `read`/`write` block when empty/full or return partial. `run pipetest` self-pipes `pipe` to VGA.
8. **Shell `|`** — done: the line parser splits on one `|` and runs left/right with a kernel pipe between them. `cat hello | cat` (or `run cat hello | run cat`) writes `hello` into the ring and the right `cat` copies it to VGA via fd 1. Nested pipes not supported.

## Leave for later

UEFI, AHCI, FAT16, long names, **fork/COW**, a libc, a framebuffer, networking, SMP, **brk/mmap**, hard links, directories > cluster without chaining bugs, a second FAT sector when clusters exceed one sector’s map.

`fork` and a real heap (`brk`) come after exec, exit status, and pipes — not before.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
