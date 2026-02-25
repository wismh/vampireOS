# Vampire OS

MBR loads stage 2, which loads a freestanding C kernel, asks BIOS for the E820 memory map, enters long mode, maps the kernel into the higher half, and jumps to `kmain`. The kernel prints that map, builds a physical frame allocator from usable RAM, maps that RAM with 2 MiB pages and 4 KiB tails, exposes it through a higher-half direct map, then drops the low identity map. VGA, the PMM bitmap and page tables go through that map, the GDT lives in kernel BSS with user-mode selectors and a TSS, the kernel runs on an HHDM stack, runs a small kernel heap, installs a 64-bit IDT, remaps the PIC so IRQs start at vector 32, runs the PIT tick counter, `iretq`s into ring-3 tasks at `0x400000` (each with a private CR3 so A/B/C/echo share that vaddr), copies user `write` strings by walking that task’s page tables through `int 0x30` (a kernel pointer from ring 3 fails), lets those tasks `yield`, `sleep` or `wait` (`wait` returns the 8-bit `exit` code in `rax`; the waiter’s row shows it once as `st N`), gives each task its own kernel stack and `TSS.RSP0`, preempts ready tasks on IRQ0, returns to a kernel idle loop on `exit` of the last task, serves `hello` / `motd` / `echo` / `a` / `b` / `c` / `opentest` / `readtest` / `cat` / `ls` / `exectest` / `status` / `waiter` / `pipetest` from a FAT12 volume after the kernel, read and written by ATA PIO from directory entries and clusters, and runs `help` / `ls` / `mem` / `cat` / `run` / `put` / `rm` from a PS/2 line buffer (a line with `|` runs the left ELF with fd 1 on the pipe write end and the right ELF with fd 0 on the read end; `cat hello | cat` prints `blood`). Boot tasks A/B/C load as ELFs from that volume the same way `run` does. Each ELF maps at `0x400000` plus a stack page in that task’s cloned PML4. Up to eight tasks; exit tears down user PTEs and page-table pages (cloned PML4 kept until slot reuse); kernel stack freed on DEAD reuse so `run` loops do not drain free frames. Each task runs under its own cloned PML4 (`CR3` switches on schedule; idle and syscalls use the boot map). `run <name>` loads any ring-3 ELF from that volume into a free slot at the shared link address. Ring-3 `open` / `close` / `read` keep a four-slot per-task fd table; `write` still takes a user string for VGA, or a small fd with buffer/length for file/`fs_write` (`run readtest` opens `hello` and prints `blood`; `run cat hello` runs the user `cat` ELF with argv on the user stack; `run ls` lists the cwd from ring 3 via `readdir`; `run exectest` `exec`s into `echo` in the same task slot; `run status` exits 42 and `run waiter` writes that code from `wait`; `pipe` fills two user ints from `rdi` (read end, write end) backed by a one-page kernel ring, `run pipetest` writes a short string through that ring then prints it; `cat hello | cat` does the same across two tasks). `put hello fang` overwrites `hello` on disk. `put note dusk` allocates a free cluster and a new directory entry. `rm note` frees that cluster and marks the directory entry deleted. `mv note dusk` renames that entry in the same parent directory. `fill note 600` writes a 600-byte FAT chain. `mkdir sub` / `cd sub` / `cd ..` walk a subdirectory. `pwd` prints `/` or `/sub`. `rmdir sub` removes an empty directory. A subdirectory that fills its first cluster grows onto the next FAT cluster. The volume root does the same after its 16 reserved slots fill. The FAT12 data region has 128 clusters so nested dirs and `fill` chains still fit. `cat sub/note` and `put sub/note dusk` use that path without changing the cwd.

## Dependencies

- [NASM](https://www.nasm.us/)
- [CMake](https://cmake.org/) 3.20+
- [LLVM](https://llvm.org/) (`clang`, `ld.lld`, `llvm-objcopy`)
- [QEMU](https://www.qemu.org/) (`qemu-system-x86_64`)

On Windows (pick one):

```text
winget install NASM.NASM SoftwareFreedomConservancy.QEMU LLVM.LLVM
choco install nasm qemu llvm
scoop install nasm qemu llvm
```

NASM, LLVM, and QEMU must be on `PATH` (or under `C:\Program Files\NASM`, `C:\Program Files\LLVM\bin`, `C:\Program Files\qemu`).

## Build

```text
cmake -B build
cmake --build build
```

This produces `build/vampire.img`.

## Run

```text
cmake --build build --target run
```
