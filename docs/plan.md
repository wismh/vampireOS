# February–April 2026

Vampire OS after `vos-64`: BIOS MBR, FAT12 on ATA PIO (128 data clusters, root chains), private CR3 per task, `mv` / argv / `run ls` / exec / 8-bit exit / pipe / shell `|`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 80 KiB (`KERNEL_SECTORS` 160); bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again. Stage 2 already loads 160 sectors in two BIOS reads (128+32).

## Now

- Volume: 128 data clusters, files up to 4 KiB. Subdirs and the root grow across FAT chains.
- Shell: `help ls mem cat run put rm mv fill mkdir rmdir cd pwd |`. Kernel `cat` reads the volume; `run cat <path>` uses the user ELF; `run ls` lists the cwd from ring 3. A line with `|` spawns left and right with a pipe between them.
- Tasks: every ELF at `0x400000`, stack at `0x401000`. Per-task cloned PML4; switch loads `task->cr3`. Exit tears down user PTEs; PML4 freed on slot reuse. `TASK_MAX` 8. `fork` copies the current task into a free slot.
- Syscalls: write (legacy string or fd), exit (8-bit code in `rdi`), yield, sleep, wait (`rdi` 0 any child / `rdi` = pid that child; 8-bit code or -1 in `rax`), open, close, read, readdir, exec, pipe (`rdi` = user `int fd[2]`; fd[0] read, fd[1] write; `rax` 0 or -1), brk (`rdi` = new break, 0 queries; `rax` the break or -1), fork (child 0 / parent child-id in `rax`), dup2 (`rdi` oldfd, `rsi` newfd; `rax` newfd or -1). Four fds per task. `run` loads any FAT12 ELF into a free slot.
- **Argv:** `run` pushes `argc` / `argv[]` / NULL on the user stack before start. `cat.asm` reads `argv[1]`. `exec` does the same for the new image.
- **readdir:** syscall copies cwd names into a user buffer; `user/ls.asm` writes them. Kernel `ls` still works.
- **exec:** syscall loads an ELF over the current task (same slot, same CR3/kstack). `run exectest` becomes `echo` without growing the task count.
- **Exit status:** `exit` takes an 8-bit code; `wait` returns it in `rax`. `rdi` 0 reaps any child of the caller; `rdi` = pid reaps only that child. The waiter’s VGA row shows `st N` once per distinct code. `run waitpid` forks two children that exit 1 and 2 and prints both codes.
- **pipe:** syscall 11 writes two fds into user `int fd[2]` via `rdi`. One-page kernel ring. `read`/`write` block when empty/full or return partial. `run pipetest` self-pipes `pipe` onto VGA.
- **Shell `|`:** one `|` splits the line; left runs with fd 1 on the write end, right with fd 0 on the read end. `cat hello | cat` and `run cat hello | run cat` copy `hello` through the ring onto VGA. Nested pipes not supported.
- **cwd:** each task stores its own cluster; `fork` copies it. The kernel shell has a separate cwd; `cd` changes that, and `run` snapshots it into the new ELF. Two tasks in different dirs list different names via `readdir`.
- **brk:** syscall 12 grows or shrinks the heap past the stack page (map from `0x402000`, or unmap on a lower break). `run brktest` stores a byte above `0x401000` and writes it.
- **fork:** syscall 13 eager-copies the current task into a free slot (cloned PML4, copied user pages including heap, copied fds, own kernel stack). Child returns 0 in `rax`; parent returns the child slot id. `run forktest` prints from both.
- **dup2:** syscall 14 remaps an fd onto another slot. The source stays open; the target is replaced. Pipe ends bump `rrefs` / `wrefs`. `run dup2test` writes through the remapped fd and those bytes show on VGA.
- **wait / waitpid:** syscall 5 takes `rdi` 0 (any child) or a child slot id. `fork` records the parent so wait only reaps that task’s children. `run waitpid` prints both exit codes.
- No eight fds, long names, second FAT sector, UEFI, AHCI.

## Sprint 1 — process and heap

`fork` and a real heap (`brk`) come after exec, exit status, and pipes — not before. Those shipped in February; this month starts there.

### Week 1 — a heap and a child

User memory has a heap past the stack page. `run` can still start a fresh ELF; `fork` also copies a running one.

1. **`brk`** — done: `SYS_BRK` (syscall 12) maps or unmaps user heap pages past the stack page. `rdi` is the new break (0 queries); `rax` returns it or -1. Heap starts at `0x402000`. `run brktest` stores a byte above `0x401000` and writes `brk`.
2. **`fork`** — done: `SYS_FORK` (syscall 13) eager-copies the current task into a free slot (cloned PML4, copied user pages, copied fds). Child returns 0 in `rax`; parent returns the child id. `user/forktest.asm` prints from both; `run forktest` leaves two VGA lines that a single ELF could not print yesterday.

### Week 2 — rewire fds and cwd

A child can remap a pipe end with `dup2`; each task has its own cwd.

3. **`dup2`** — done: `SYS_DUP2` (syscall 14) remaps an fd onto another slot. `rdi` is oldfd, `rsi` is newfd; `rax` returns newfd or -1. The source stays open; the target is replaced. `run dup2test` writes through the remapped fd and those bytes show on VGA.
4. **Per-task cwd** — done: each task stores its own cwd cluster; `fork` copies the parent’s. The kernel shell keeps a separate cwd; `cd` changes that, and `run` snapshots it into the new ELF. After `mkdir a` / `mkdir b`, `cd a` then `run ls` lists `a`’s names and `cd b` then `run ls` lists `b`’s.

### Week 3 — reap children and more fds

`wait` can reap a chosen child. Four fds fill once a pipe and two files are open.

5. **`wait` / waitpid** — done: after fork, more than one child can be DEAD. `wait` with `rdi` 0 reaps any child; `wait` with `rdi` set returns only that child’s 8-bit code in `rax`. `run waitpid` forks two children that exit 1 and 2; the parent prints both codes.
6. **Eight fds** — raise `FD_MAX` from 4 to 8 so stdin/stdout, a pipe pair, and open files can coexist. `run fdtest` opens enough files (or a pipe plus files) that the fifth `open` no longer returns -1; a line on VGA shows the extra fds.

### Week 4 — a pipe without the kernel `|`, and `ps`

The kernel line parser is not how user code should connect a child. Fork is invisible except as extra VGA rows.

7. **Userspace pipe via fork** — `user/pipefork.asm`: `pipe`, `fork`, child writes, parent reads. No kernel `|`. `run pipefork` prints the child’s string on VGA through the ring.
8. **`ps`** — kernel shell lists live slots / pids (id, state, maybe name). After `run forktest`, `ps` shows the extra slot so fork is visible without VGA row archaeology.

## Sprint 2 — memory and files

Eager fork copies every user page. File fds always read from offset 0. The FAT still fits in one sector.

### Week 1 — copy on write

Parent and child already diverge only because the copy was full. Stop paying that until a write.

9. **COW fork** — mark copied user pages read-only and share the frames; do not duplicate writable pages until a write fault. `mem` (free frames) after `run forktest` must not drop by a full extra code+stack+heap the way eager copy did.
10. **Write-fault copy** — the `#PF` path allocates a private frame, maps it writable, resumes. Parent and child store different bytes on the heap; `run cowtest` prints two different values.

### Week 2 — seek and more clusters

`read` always starts at byte 0. One FAT sector maps ~170 clusters; 128 data clusters waste none of that yet, and cannot grow past it.

11. **`lseek`** — file fds keep an offset; `SYS_LSEEK` sets it. `run readtest` (or a new ELF) seeks into `hello` and prints from the middle (`ood` from `blood`, or similar).
12. **Second FAT sector** — `FAT_SEC_PER_FAT` 2 (both copies); grow `FAT_DATA_CLUSTERS` past one sector’s map so more than ~170 clusters can exist. `fill` (or many `put`s) uses a cluster that would not fit in 512 bytes of FAT12 entries; `ls` still lists the file.

### Week 3 — copy and `stat`

Copying a file is still `cat` plus `put` by hand. User code cannot ask size or whether a name is a directory.

13. **`cp`** — shell `cp src dst` copies a file on the volume (new clusters + dirent). `cp hello dusk` then `cat dusk` prints `blood`. `cp` of a missing src prints `?`.
14. **`stat`** — syscall fills a small user struct (size, first cluster / is-dir). Pack `user/stat.asm`; `run stat hello` prints the size.

### Week 4 — kill a slot

The only way a slot dies is `exit` from inside it.

15. **`kill`** — syscall marks another slot DEAD with a status; `ps` drops it. `run` a sleeper, `kill` that pid from the shell (or `run kill <id>`); `ps` no longer lists it and the prompt still accepts `help`.
16. **Ctrl+C** — PS/2 Ctrl+C kills the current `run` foreground task (or the last spawned ELF) as if `kill`; the kernel line buffer stays. `run` a sleeper, send Ctrl+C; `ps` no longer lists it and a new line still takes `ls`.

## Sprint 3 — userland

Every user program is still NASM. The only shell is the kernel line buffer. Do not jump to UEFI, AHCI, or SMP.

### Week 1 — C from the volume

Ring 3 cannot call `int 0x30` from C yet.

17. **CRT stubs** — `user/crt.asm` (or similar) with C-callable `write` / `read` / `exit` / `fork` / `brk` / `wait` wrapping `int 0x30`. A tiny program linked against it prints a line; `run crt` (or the next slice’s `hi`) shows that text on VGA.
18. **First C program** — `user/hi.c` compiled freestanding with the existing clang/ld.lld pipeline, packed on FAT12. `run hi` prints a line.

### Week 2 — a user shell

`run` still starts programs from the kernel prompt.

19. **User shell ELF** — a small `user/sh.asm` or `user/sh.c` that reads a line (or takes argv) and `exec`s a program from the volume. `run sh` then `hi` (or `echo`) prints that program’s line.
20. **Boot to `sh`** — after kernel init, `exec` / `run` the user shell instead of only the kernel line buffer. Keep kernel `help` / `ls` as fallback if needed. Prove `run sh` or that the first prompt is the user shell (a `$` or similar the kernel buffer did not print yesterday).

### Week 3 — another map and string helpers

User space is still code + stack + `brk` heap. `hi` / `sh` should not open-code `strlen`.

21. **Anonymous mmap** — map one more page at a chosen user address (`SYS_MMAP`; `brk` of a full page is not enough — a distinct mapping, not the heap). `run mmaptest` writes a byte there and prints it.
22. **Tiny libc string/mem** — `memcpy` / `strlen` / `strcmp` used by `hi` or `sh`. `run hi` (or `run sh` with an argv compare) prints a line those helpers produced.

### Week 4 — time and `init`

The PIT already ticks; nothing prints the time. Killing `sh` would idle the kernel.

23. **PIT clock** — shell `date` or `uptime` prints tick-derived seconds. After boot, `uptime` shows a small integer that grows if you `sleep` and ask again.
24. **`init`** — a reaper task that `wait`s forever and restarts `sh` when it exits, so killing the shell does not idle the kernel. `kill` the shell pid (or `exit` from `sh`); a new `sh` prompt appears and `ps` still lists `init`.

## Leave for later

UEFI, AHCI, SMP, networking, a real libc stdio, FAT16, long names, hard links, a framebuffer.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
