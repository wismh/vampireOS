# Vampire OS

Minimal x86-64 BIOS boot: MBR loads stage 2, which enters long mode and prints `Vampire OS`.

## Dependencies

- [NASM](https://www.nasm.us/)
- [CMake](https://cmake.org/) 3.20+
- [QEMU](https://www.qemu.org/) (`qemu-system-x86_64`)

On Windows (pick one):

```text
winget install NASM.NASM SoftwareFreedomConservancy.QEMU
choco install nasm qemu
scoop install nasm qemu
```

NASM and QEMU must be on `PATH` (or under `C:\Program Files\NASM` / `C:\Program Files\qemu`).

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
