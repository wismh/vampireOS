# Vampire OS

Minimal x86 BIOS bootloader: a 512-byte MBR that prints `Vampire OS` and halts.

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

This produces `build/vampire.img` (the boot sector).

## Run

```text
cmake --build build --target run
```
