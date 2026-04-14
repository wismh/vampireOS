# February–April 2026

Vampire OS after `vos-64`: BIOS MBR, FAT12 on ATA PIO (344 data clusters, two FAT sectors, root chains), private CR3 per task, `mv` / argv / `run ls` / exec / 8-bit exit / pipe / shell `|`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 88 KiB (`KERNEL_SECTORS` 176); bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again. Stage 2 already loads 176 sectors in two BIOS reads (128+48).

## Now

- Volume: 344 data clusters, two sectors per FAT (both copies). Files up to 4 KiB. Subdirs and the root grow across FAT chains.
- Shell: `help ls mem cat run put rm mv cp fill mkdir rmdir cd pwd ps kill uptime |`. Kernel `cat` reads the volume; `run cat <path>` uses the user ELF; `run ls` lists the cwd from ring 3. A line with `|` spawns left and right with a pipe between them. `ps` lists live slots (id, optional name, and RUN/SLEEP/WAIT). `kill <id>` marks that slot DEAD. Ctrl+C on the PS/2 path kills the last `run` ELF the same way and leaves the line buffer. `run crt` prints `crt` through C-callable `int 0x30` stubs. `run hi` prints `hi` from freestanding C via `memcpy` / `strlen` / `strcmp`. After kernel init the image `run`s `init`, which `fork`/`exec`s `sh`; the first prompt is `$`. Type `hi` (no `run sh`) to print `hi`. `exit` (or `kill` the shell pid) makes `init` start `sh` again. Kernel `help` / `ls` remain as fallback. `run sh` then `hi` (typed, or `run sh hi`) still `exec`s that volume ELF (`sh` `strcmp`s argv[1]). Kernel `uptime` (and `$` `uptime` / `run uptime`) prints seconds from the PIT tick count.
- Tasks: every ELF at `0x400000`, stack at `0x401000`. Per-task cloned PML4; switch loads `task->cr3`. Exit tears down user PTEs; PML4 freed on slot reuse. `TASK_MAX` 8. `fork` shares the current task’s user frames as read-only until a write.
- Syscalls: write (legacy string or fd), exit (8-bit code in `rdi`), yield, sleep, wait (`rdi` 0 any child / `rdi` = pid that child; 8-bit code or -1 in `rax`), open, close, read, readdir, exec, pipe (`rdi` = user `int fd[2]`; fd[0] read, fd[1] write; `rax` 0 or -1), brk (`rdi` = new break, 0 queries; `rax` the break or -1), fork (child 0 / parent child-id in `rax`), dup2 (`rdi` oldfd, `rsi` newfd; `rax` newfd or -1), lseek (`rdi` fd, `rsi` offset; SEEK_SET; `rax` the new offset or -1), stat (`rdi` path, `rsi` user `{size, cluster, is_dir}` packed ints; `rax` 0 or -1), kill (`rdi` pid, `rsi` 8-bit status; `rax` 0 or -1; the slot is DEAD so `ps` skips it and wait can reap it), mmap (`rdi` hint VA, `rsi` length; one anonymous user page at that address, not the `brk` heap; `rax` the mapped VA or -1), uptime (`rax` = PIT seconds, `idt_ticks() / 100`). Eight fds per task. `run` loads any FAT12 ELF into a free slot.
- **Argv:** `run` pushes `argc` / `argv[]` / NULL on the user stack before start. `cat.asm`, `stat.asm`, and `kill.asm` read `argv[1]`. `exec` does the same for the new image.
- **readdir:** syscall copies cwd names into a user buffer; `user/ls.asm` writes them. Kernel `ls` still works.
- **exec:** syscall loads an ELF over the current task (same slot, same CR3/kstack). `run exectest` becomes `echo` without growing the task count.
- **Exit status:** `exit` takes an 8-bit code; `wait` returns it in `rax`. `rdi` 0 reaps any child of the caller; `rdi` = pid reaps only that child. The waiter’s VGA row shows `st N` once per distinct code. `run waitpid` forks two children that exit 1 and 2 and prints both codes.
- **pipe:** syscall 11 writes two fds into user `int fd[2]` via `rdi`. One-page kernel ring. `read`/`write` block when empty/full or return partial. `run pipetest` self-pipes `pipe` onto VGA.
- **Shell `|`:** one `|` splits the line; left runs with fd 1 on the write end, right with fd 0 on the read end. `cat hello | cat` and `run cat hello | run cat` copy `hello` through the ring onto VGA. Nested pipes not supported.
- **cwd:** each task stores its own cluster; `fork` copies it. The kernel shell has a separate cwd; `cd` changes that, and `run` snapshots it into the new ELF. Two tasks in different dirs list different names via `readdir`.
- **brk:** syscall 12 grows or shrinks the heap past the stack page (map from `0x402000`, or unmap on a lower break). `run brktest` stores a byte above `0x401000` and writes it.
- **fork:** syscall 13 shares the current task’s user frames as read-only into a free slot (cloned PML4, shared pages, copied fds, own kernel stack). A write `#PF` copies that frame privately. Child returns 0 in `rax`; parent returns the child slot id. `run forktest` prints from both; `mem` does not drop a full extra code+stack+heap.
- **dup2:** syscall 14 remaps an fd onto another slot. The source stays open; the target is replaced. Pipe ends bump `rrefs` / `wrefs`. `run dup2test` writes through the remapped fd and those bytes show on VGA.
- **wait / waitpid:** syscall 5 takes `rdi` 0 (any child) or a child slot id. `fork` records the parent so wait only reaps that task’s children. `run waitpid` prints both exit codes.
- **Eight fds:** `FD_MAX` is 8. `run fdtest` opens `hello` five times; the fifth `open` returns fd 4 (not -1) and that digit shows on VGA.
- **pipefork:** `user/pipefork.asm` calls `pipe` then `fork`; the child writes a string, the parent reads it and writes VGA fd 1. `run pipefork` prints that string through the ring. No kernel `|`.
- **ps:** kernel shell lists live slots (id, optional name, and RUN/SLEEP/WAIT; DEAD skipped). After `run forktest`, the extra child slot is visible without reading VGA rows. Boot `init` shows as `init` so a restart still lists the reaper.
- **cowtest:** `user/cowtest.asm` `brk`s a heap page, `fork`s, then parent and child store different bytes at that address. A write `#PF` copies the frame privately. `run cowtest` prints two different values.
- **lseek:** syscall 15 sets a file fd's offset (`rdi` fd, `rsi` offset; SEEK_SET). `read` starts there and advances it. `run readtest` seeks into `hello` and prints `ood`.
- **Second FAT sector:** `FAT_SEC_PER_FAT` is 2; both copies are 1024 bytes. `fill` can occupy a cluster whose FAT12 entry sits past the first 512 bytes; `ls` still lists that file.
- **cp:** kernel shell `cp src dst` copies a file onto a new dirent and new clusters. `cp hello dusk` then `cat dusk` prints `blood`. A missing src prints `?`.
- **stat:** syscall 16 fills a user `{size, first cluster, is-dir}` packed-int struct (`rdi` path, `rsi` struct). `run stat hello` prints `5`.
- **kill:** syscall 17 marks another slot DEAD with an 8-bit status (`rdi` pid, `rsi` status). `run sleeper` then `kill <id>` (or `run kill <id>`) drops that slot from `ps`. The kernel prompt still takes `help`.
- **Ctrl+C:** PS/2 Ctrl held + `c` kills the last `run` (or pipeline) ELF via the same DEAD path; the kernel line buffer stays so a new line still takes `ls`.
- **CRT:** `user/crt.asm` exports C-callable `write` / `read` / `exit` / `fork` / `brk` / `wait` / `exec` (args in rdi, rsi, rdx; return in rax) wrapping `int 0x30`. `user/crtstart.asm` is linked against that object; `run crt` prints `crt`.
- **First C program:** `user/hi.c` is freestanding C (`int main(void)` `memcpy`s a string, `strcmp`s the copy, then `write`s `strlen` bytes). Linked with `user/crt0.asm`, `user/string.c`, and the CRT stubs via clang/ld.lld; packed as `HI`. `run hi` prints `hi`.
- **User shell:** `user/sh.c` reads a line from fd 0 (PS/2 stdin) or `strcmp`s argv[1] and `exec`s a volume ELF (`exit` ends the slot). Packed as `SH`. Boot `run`s `init`, which starts `sh`; the first prompt is `$`. Type `hi` (no `run sh`) prints `hi`. `kill` the shell pid or type `exit`; a new `$` appears and `ps` still lists `init`. `run sh` then `hi` (or `run sh hi`) still works. Kernel `help` / `ls` remain as fallback.
- **mmap:** syscall 18 maps one anonymous user page at a chosen address (`rdi` hint, `rsi` length; `rax` the VA or -1). Distinct from the `brk` heap. `run mmaptest` stores a byte at `0x500000` and writes `mmap`.
- **Tiny libc:** `user/string.c` provides `memcpy` / `strlen` / `strcmp`. `hi` and `sh` call those helpers (not open-coded loops). `run hi` prints a line they produced.
- **PIT clock:** syscall 19 returns seconds from the PIT tick count (`idt_ticks() / 100`; PIT at 100 Hz). Kernel `uptime` prints that integer; `$` `uptime` / `run uptime` does the same from the packed `UPTIME` ELF. After boot the value is small; after `sleep` it is larger.
- **init:** `user/init.c` `fork`/`exec`s `sh`, `wait`s, and starts `sh` again when that child exits. Packed as `INIT`. Boot `run`s `init` after kernel init. `ps` names that slot `init`. Killing the shell (or `exit`) prints a new `$` without a reboot.
- No long names, UEFI, AHCI.

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

`wait` can reap a chosen child. Eight fds so stdin/stdout, a pipe pair, and open files can coexist.

5. **`wait` / waitpid** — done: after fork, more than one child can be DEAD. `wait` with `rdi` 0 reaps any child; `wait` with `rdi` set returns only that child’s 8-bit code in `rax`. `run waitpid` forks two children that exit 1 and 2; the parent prints both codes.
6. **Eight fds** — done: `FD_MAX` is 8 so stdin/stdout, a pipe pair, and open files can coexist. `run fdtest` opens `hello` five times; the fifth `open` returns fd 4 (not -1) and that digit shows on VGA.

### Week 4 — a pipe without the kernel `|`, and `ps`

The kernel line parser is not how user code should connect a child. Fork is invisible except as extra VGA rows.

7. **Userspace pipe via fork** — done: `user/pipefork.asm` calls `pipe` then `fork`; the child writes, the parent reads. No kernel `|`. `run pipefork` prints the child’s string on VGA through the ring.
8. **`ps`** — done: kernel shell lists live slots (id and RUN/SLEEP/WAIT; DEAD skipped). After `run forktest`, `ps` shows the extra slot so fork is visible without VGA row archaeology.

## Sprint 2 — memory and files

Eager fork copied every user page. File fds kept no offset. One FAT sector could not map past ~170 clusters.

### Week 1 — copy on write

Parent and child already diverge only because the copy was full. Stop paying that until a write.

9. **COW fork** — done: fork marks user pages read-only and shares the frames. A write `#PF` copies the frame privately and resumes. `mem` after `run forktest` does not drop a full extra code+stack+heap.
10. **Write-fault copy** — done: parent and child store different bytes on the heap; `run cowtest` prints two different values. (The `#PF` copy path shipped with COW fork.)

### Week 2 — seek and more clusters

File fds keep an offset. Each FAT is two sectors so a cluster past the first 512 bytes of FAT12 entries can exist.

11. **`lseek`** — done: file fds keep an offset; `SYS_LSEEK` (syscall 15) sets it (`rdi` fd, `rsi` offset; SEEK_SET; `rax` the new offset or -1). `read` starts at that offset and advances it. `run readtest` seeks into `hello` and prints `ood` from `blood`.
12. **Second FAT sector** — done: `FAT_SEC_PER_FAT` 2 (both copies); `FAT_DATA_CLUSTERS` 344 so a FAT12 entry can sit past one sector’s map. `fill` uses a cluster whose entry lives in the second FAT sector; `ls` still lists the file.

### Week 3 — copy and `stat`

A file can be copied on the volume. User code cannot ask size or whether a name is a directory.

13. **`cp`** — done: shell `cp src dst` copies a file on the volume (new clusters + dirent). `cp hello dusk` then `cat dusk` prints `blood`. `cp` of a missing src prints `?`.
14. **`stat`** — done: syscall 16 fills a small user struct (size, first cluster, is-dir packed ints; `rdi` path, `rsi` struct; `rax` 0 or -1). Pack `user/stat.asm`; `run stat hello` prints `5`.

### Week 4 — kill a slot

The only way a slot dies is `exit` from inside it.

15. **`kill`** — done: syscall 17 marks another slot DEAD with an 8-bit status (`rdi` pid, `rsi` status; `rax` 0 or -1). Pack `user/sleeper.asm`; `run sleeper` then kernel `kill <id>` (or `run kill <id>`) so `ps` no longer lists it and `help` still works.
16. **Ctrl+C** — done: PS/2 Ctrl held + `c` kills the last `run` foreground ELF via `sched_kill_slot` (same DEAD path as `kill`). The kernel line buffer stays. `run sleeper`, Ctrl+C; `ps` no longer lists it and a new line still takes `ls`.

## Sprint 3 — userland

Every user program is still NASM. The only shell is the kernel line buffer. Do not jump to UEFI, AHCI, or SMP.

### Week 1 — C from the volume

Ring 3 can call `write` from C through the CRT stubs.

17. **CRT stubs** — done: `user/crt.asm` exports C-callable `write` / `read` / `exit` / `fork` / `brk` / `wait` wrapping `int 0x30`. `user/crtstart.asm` is linked against that object with the clang/ld.lld pipeline; `run crt` prints `crt`.
18. **First C program** — done: `user/hi.c` compiled freestanding with clang (`-ffreestanding -fno-stack-protector`) and linked with `crt0` plus the vos-82 CRT stubs via ld.lld; packed as `HI`. `run hi` prints `hi`.

### Week 2 — a user shell

`run` still starts programs from the kernel prompt. Boot now starts `sh`.

19. **User shell ELF** — done: `user/sh.c` reads a line (fd 0 / `read`) or takes argv and `exec`s a volume program. Packed as `SH`. `run sh` then `hi` prints `hi`. Kernel `help` / `ls` still work.
20. **Boot to `sh`** — done: after kernel init, `user_run("sh")` starts the user shell. First prompt is `$`. Type `hi` (no `run sh`) prints `hi`. Kernel `help` / `ls` still work as fallback.

### Week 3 — another map and string helpers

User space is still code + stack + `brk` heap plus `mmap`. `hi` / `sh` call tiny libc string helpers.

21. **Anonymous mmap** — done: `SYS_MMAP` (syscall 18) maps one anonymous user page at a chosen address (`rdi` hint, `rsi` length of one page; `rax` the mapped VA or -1). Distinct from the `brk` heap at `0x402000`. `run mmaptest` stores a byte at `0x500000` and writes `mmap`.
22. **Tiny libc string/mem** — done: `user/string.c` exports `memcpy` / `strlen` / `strcmp`. `hi` copies `"hi"` with `memcpy`, checks it with `strcmp`, and `write`s `strlen` bytes; `sh` `strcmp`s argv[1] before `exec`. `run hi` prints `hi`.

### Week 4 — time and `init`

The PIT already ticks; nothing prints the time. Killing `sh` would idle the kernel.

23. **PIT clock** — done: shell `uptime` prints seconds from the PIT tick count (`idt_ticks() / 100`; PIT at 100 Hz). Syscall 19 (`SYS_UPTIME`) returns those seconds in `rax`. After boot, `$` `uptime` (or kernel `uptime`) shows a small integer; after `sleep` it is larger. Packed as `UPTIME`.
24. **`init`** — done: after kernel init, `user_run("init")` starts a reaper that `fork`/`exec`s `sh` and `wait`s. First prompt is `$`. `kill` the shell pid (or type `exit`); a new `$` appears and `ps` still lists `init`.

## Leave for later

UEFI, AHCI, SMP, networking, a real libc stdio, FAT16, long names, hard links, a framebuffer.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
