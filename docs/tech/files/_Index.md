---
tags: [moc, files]
aliases: [File index]
---

# File index

One note per boot/kernel source, CMake glue, and first-party C userland. NASM test ELFs are listed without a note each.

Hubs: [[Home]] · [[architecture/Overview]] · [[build/CMake]]

## Boot, kernel, build, C userland

| Path | Note | Module |
| --- | --- | --- |
| `boot/boot.asm` | [[boot.boot.asm]] | [[modules/Boot]] |
| `boot/const.inc` | [[boot.const.inc]] | [[modules/Boot]] |
| `boot/stage2.asm` | [[boot.stage2.asm]] | [[modules/Boot]] |
| `kernel/ahci.c` | [[kernel.ahci.c]] | [[modules/Block]] |
| `kernel/ahci.h` | [[kernel.ahci.h]] | [[modules/Block]] |
| `kernel/ata.c` | [[kernel.ata.c]] | [[modules/Block]] |
| `kernel/ata.h` | [[kernel.ata.h]] | [[modules/Block]] |
| `kernel/bio.c` | [[kernel.bio.c]] | [[modules/Block]] |
| `kernel/bio.h` | [[kernel.bio.h]] | [[modules/Block]] |
| `kernel/e820.c` | [[kernel.e820.c]] | [[modules/Memory]] |
| `kernel/e820.h` | [[kernel.e820.h]] | [[modules/Memory]] |
| `kernel/elf.c` | [[kernel.elf.c]] | [[modules/Memory]] |
| `kernel/elf.h` | [[kernel.elf.h]] | [[modules/Memory]] |
| `kernel/fb.c` | [[kernel.fb.c]] | [[modules/Console]] |
| `kernel/fb.h` | [[kernel.fb.h]] | [[modules/Console]] |
| `kernel/fs.c` | [[kernel.fs.c]] | [[modules/FS]] |
| `kernel/fs.h` | [[kernel.fs.h]] | [[modules/FS]] |
| `kernel/heap.c` | [[kernel.heap.c]] | [[modules/Memory]] |
| `kernel/heap.h` | [[kernel.heap.h]] | [[modules/Memory]] |
| `kernel/idt.c` | [[kernel.idt.c]] | [[modules/Tasks]] |
| `kernel/idt.h` | [[kernel.idt.h]] | [[modules/Tasks]] |
| `kernel/io.h` | [[kernel.io.h]] | [[modules/Console]] |
| `kernel/isr.asm` | [[kernel.isr.asm]] | [[modules/Tasks]] |
| `kernel/kbd.c` | [[kernel.kbd.c]] | [[modules/Console]] |
| `kernel/kbd.h` | [[kernel.kbd.h]] | [[modules/Console]] |
| `kernel/kmain.c` | [[kernel.kmain.c]] | [[modules/Boot]] |
| `kernel/linker.ld` | [[kernel.linker.ld]] | [[modules/Memory]] |
| `kernel/mouse.c` | [[kernel.mouse.c]] | [[modules/Console]] |
| `kernel/mouse.h` | [[kernel.mouse.h]] | [[modules/Console]] |
| `kernel/pack.asm` | [[kernel.pack.asm]] | [[modules/Memory]] |
| `kernel/pic.c` | [[kernel.pic.c]] | [[modules/Tasks]] |
| `kernel/pic.h` | [[kernel.pic.h]] | [[modules/Tasks]] |
| `kernel/pit.c` | [[kernel.pit.c]] | [[modules/Tasks]] |
| `kernel/pit.h` | [[kernel.pit.h]] | [[modules/Tasks]] |
| `kernel/pmm.c` | [[kernel.pmm.c]] | [[modules/Memory]] |
| `kernel/pmm.h` | [[kernel.pmm.h]] | [[modules/Memory]] |
| `kernel/rtc.c` | [[kernel.rtc.c]] | [[modules/Tasks]] |
| `kernel/rtc.h` | [[kernel.rtc.h]] | [[modules/Tasks]] |
| `kernel/sched.c` | [[kernel.sched.c]] | [[modules/Tasks]] |
| `kernel/sched.h` | [[kernel.sched.h]] | [[modules/Tasks]] |
| `kernel/serial.c` | [[kernel.serial.c]] | [[modules/Console]] |
| `kernel/serial.h` | [[kernel.serial.h]] | [[modules/Console]] |
| `kernel/user.c` | [[kernel.user.c]] | [[modules/Tasks]] |
| `kernel/user.h` | [[kernel.user.h]] | [[modules/Tasks]] |
| `kernel/vga.c` | [[kernel.vga.c]] | [[modules/Console]] |
| `kernel/vga.h` | [[kernel.vga.h]] | [[modules/Console]] |
| `kernel/virtio.c` | [[kernel.virtio.c]] | [[modules/Block]] |
| `kernel/virtio.h` | [[kernel.virtio.h]] | [[modules/Block]] |
| `kernel/vmm.c` | [[kernel.vmm.c]] | [[modules/Memory]] |
| `kernel/vmm.h` | [[kernel.vmm.h]] | [[modules/Memory]] |
| `CMakeLists.txt` | [[CMakeLists.txt]] | [[build/CMake]] |
| `cmake/embed_bin.cmake` | [[cmake.embed_bin.cmake]] | [[build/CMake]] |
| `cmake/concat.cmake` | [[cmake.concat.cmake]] | [[build/CMake]] |
| `user/user.ld` | [[user.user.ld]] | [[modules/Userland]] |
| `user/crt0.asm` | [[user.crt0.asm]] | [[modules/Userland]] |
| `user/crtstart.asm` | [[user.crtstart.asm]] | [[modules/Userland]] |
| `user/crt.asm` | [[user.crt.asm]] | [[modules/Userland]] |
| `user/fd.asm` | [[user.fd.asm]] | [[modules/Userland]] |
| `user/string.c` | [[user.string.c]] | [[modules/Userland]] |
| `user/printf.c` | [[user.printf.c]] | [[modules/Userland]] |
| `user/malloc.c` | [[user.malloc.c]] | [[modules/Userland]] |
| `user/font.c` | [[user.font.c]] | [[modules/Userland]] |
| `user/font.h` | [[user.font.h]] | [[modules/Userland]] |
| `user/init.c` | [[user.init.c]] | [[modules/Userland]] |
| `user/sh.c` | [[user.sh.c]] | [[modules/Userland]] |
| `user/hi.c` | [[user.hi.c]] | [[modules/Userland]] |
| `user/bss.c` | [[user.bss.c]] | [[modules/Userland]] |
| `user/fbhello.c` | [[user.fbhello.c]] | [[modules/Userland]] |
| `user/malloctest.c` | [[user.malloctest.c]] | [[modules/Userland]] |

## NASM test ELFs (no note)

| Path |
| --- |
| `user/a.asm` |
| `user/b.asm` |
| `user/brktest.asm` |
| `user/c.asm` |
| `user/cat.asm` |
| `user/catch.asm` |
| `user/cowtest.asm` |
| `user/date.asm` |
| `user/dup2test.asm` |
| `user/echo.asm` |
| `user/exectest.asm` |
| `user/fat.asm` |
| `user/fb.asm` |
| `user/fbclear.asm` |
| `user/fbinfo.asm` |
| `user/fbtest.asm` |
| `user/fdtest.asm` |
| `user/forktest.asm` |
| `user/kill.asm` |
| `user/ls.asm` |
| `user/mmapfile.asm` |
| `user/mmaptest.asm` |
| `user/mprottest.asm` |
| `user/munmaptest.asm` |
| `user/opentest.asm` |
| `user/pipefork.asm` |
| `user/pipetest.asm` |
| `user/readtest.asm` |
| `user/sleeper.asm` |
| `user/stat.asm` |
| `user/status.asm` |
| `user/uptime.asm` |
| `user/waiter.asm` |
| `user/waitpid.asm` |
