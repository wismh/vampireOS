# Vampire OS

MBR loads stage 2, which loads a freestanding C kernel, asks BIOS for the E820 memory map, enters long mode, maps the kernel into the higher half, and jumps to `kmain`. The kernel prints that map, builds a physical frame allocator from usable RAM, maps that RAM with 2 MiB pages and 4 KiB tails, exposes it through a higher-half direct map, then drops the low identity map. VGA, the PMM bitmap and page tables go through that map, the GDT lives in kernel BSS with user-mode selectors and a TSS, the kernel runs on an HHDM stack, runs a small kernel heap, installs a 64-bit IDT, remaps the PIC so IRQs start at vector 32, runs the PIT tick counter, `iretq`s into a ring-3 page at `0x400000`, copies a user `write` string through `int 0x30`, returns to a kernel idle loop on `exit`, and runs `help` / `mem` from a PS/2 line buffer on that heap.

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
