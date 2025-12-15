# Vampire OS

MBR loads stage 2, which loads a freestanding C kernel, asks BIOS for the E820 memory map, enters long mode, maps the kernel into the higher half, and jumps to `kmain`. The kernel prints that map, builds a physical frame allocator from usable RAM, maps that RAM with 2 MiB pages and 4 KiB tails, exposes it through a higher-half direct map, then drops the low identity map. VGA, the PMM bitmap and page tables go through that map, the GDT lives in kernel BSS with user-mode selectors and a TSS, the kernel runs on an HHDM stack, runs a small kernel heap, installs a 64-bit IDT, remaps the PIC so IRQs start at vector 32, runs the PIT tick counter, `iretq`s into ring-3 tasks at `0x400000`, `0x402000` and `0x404000`, copies user `write` strings through `int 0x30`, lets those tasks `yield`, `sleep` or `wait`, gives each task its own kernel stack and `TSS.RSP0`, preempts ready tasks on IRQ0, returns to a kernel idle loop on `exit` of the last task, serves `hello` / `motd` / `echo` from an initrd loaded after the kernel on disk into ramfs, and runs `help` / `ls` / `mem` / `cat` / `run` from a PS/2 line buffer. `run echo` loads a ring-3 ELF from that ramfs.

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
