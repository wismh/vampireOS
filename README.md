# Vampire OS

A hobby **x86_64** OS: BIOS MBR boot, a freestanding C kernel, and a small userland. It boots in QEMU to a `$` shell.

Internals: **[docs/tech](docs/tech/README.md)** (architecture, modules, syscalls, disk layout, file index).

## Snapshot

What is on the image today (after `vos-151`):

| Area | What you get |
| --- | --- |
| Boot | MBR → stage 2 → long mode. COM1 prints `boot` before `kmain`. VBE 640×480×32 when the BIOS offers it. |
| Memory | PMM + HHDM, private CR3 per task, COW `fork`, `brk` / `mmap` / `munmap` / `mprotect`. |
| Tasks | 16 slots, eight fds, pipes, `wait` / `kill` / `sigaction` (SIGINT / SIGTERM). ELFs at `0x400000`. |
| Volume | FAT12 in an MBR partition at LBA **273** (VFAT LFN, hard links). Backends: VirtIO-blk → AHCI → ATA. |
| Shell | `init` → `sh`: nested `\|`, one `<` `>` or `>>`, `&`, builtins `cd` / `pwd`. |
| Console | VGA 80×25, LFB overlay of the prompt, COM1 mirror, PS/2 mouse click prints `x,y`. |
| Net | virtio-net: print MAC, send one UDP `hi` to `10.0.2.2:5555`, then stop. |
| Time | `$ date` is CMOS at boot plus PIT seconds. `$ uptime` is PIT only. |

Not there: UEFI, SMP, TCP/DHCP/sockets, FAT32, job control (`fg` / `bg`).

## Dependencies

- [NASM](https://www.nasm.us/)
- [CMake](https://cmake.org/) 3.20+
- [LLVM](https://llvm.org/) (`clang`, `ld.lld`, `llvm-objcopy`)
- [QEMU](https://www.qemu.org/) (`qemu-system-x86_64`)

Windows (pick one):

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

That writes `build/vampire.img`.

## Run

```text
cmake --build build --target run
```

QEMU boots the image from IDE, attaches the **same file** to AHCI and virtio-blk (`file.locking=off`, `cache=writethrough`, no `snapshot=on`), adds virtio-net, dumps packets to `build/virtio-net.pcap`, and uses `-vga std` plus `-serial stdio`.

Flags the `run` target passes:

```text
-drive format=raw,file=build/vampire.img,if=ide,cache=writethrough,file.locking=off
-device ahci,id=ahci
-drive id=ahci-hd,file=build/vampire.img,if=none,format=raw,cache=writethrough,file.locking=off
-device ide-hd,drive=ahci-hd,bus=ahci.0
-drive id=virtio-hd,file=build/vampire.img,if=none,format=raw,cache=writethrough,file.locking=off
-device virtio-blk-pci,drive=virtio-hd
-netdev user,id=net0
-object filter-dump,id=dump0,netdev=net0,file=build/virtio-net.pcap
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
-rtc base=2026-07-22T18:50:00
-serial stdio
```

### What boot should look like

- VGA: `virt 55aa`, `ahci 55aa`, `part 273`, then `$`.
- Serial: `boot` → `net 52:54:00:12:34:56` → `udp sent` → `$`.

### A few commands

```text
cat hello              # blood
hi > out && cat out    # hi 42
sleeper &              # $ returns; ps still lists the sleeper
date                   # YYYY-MM-DD HH:MM:SS
devs                   # virt ahci ata
sync                   # flush FATs + the active block device
```

`$ cat bad` (cluster past the image) prints `?` and returns to `$`. After `$ hi > virt.txt` then `$ sync`, a cold restart still `cat`s `virt.txt`.

## Kernel pad

On-disk kernel is **256** sectors; the FAT partition starts at LBA **273**. PMM still reserves **304** sectors of RAM so `.bss` does not sit on the bitmap. Bump those together when `kernel.raw.bin` no longer fits 256 sectors. Details: [docs/tech/build](docs/tech/build/README.md).
