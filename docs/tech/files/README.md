# File index

One note per boot/kernel source, CMake glue, and first-party C userland. NASM test ELFs are listed without a note each.

Hubs: [Home](../README.md) · [Architecture overview](../architecture/overview.md) · [CMake](../build/cmake.md)

## Boot, kernel, build, C userland

| Path | Note | Module |
| --- | --- | --- |
| [`boot/boot.asm`](../../../boot/boot.asm) | [boot/boot.asm](boot.boot.asm.md) | [Boot](../modules/boot.md) |
| [`boot/const.inc`](../../../boot/const.inc) | [boot/const.inc](boot.const.inc.md) | [Boot](../modules/boot.md) |
| [`boot/stage2.asm`](../../../boot/stage2.asm) | [boot/stage2.asm](boot.stage2.asm.md) | [Boot](../modules/boot.md) |
| [`kernel/ahci.c`](../../../kernel/ahci.c) | [kernel/ahci.c](kernel.ahci.c.md) | [Block](../modules/block.md) |
| [`kernel/ahci.h`](../../../kernel/ahci.h) | [kernel/ahci.h](kernel.ahci.h.md) | [Block](../modules/block.md) |
| [`kernel/ata.c`](../../../kernel/ata.c) | [kernel/ata.c](kernel.ata.c.md) | [Block](../modules/block.md) |
| [`kernel/ata.h`](../../../kernel/ata.h) | [kernel/ata.h](kernel.ata.h.md) | [Block](../modules/block.md) |
| [`kernel/bio.c`](../../../kernel/bio.c) | [kernel/bio.c](kernel.bio.c.md) | [Block](../modules/block.md) |
| [`kernel/bio.h`](../../../kernel/bio.h) | [kernel/bio.h](kernel.bio.h.md) | [Block](../modules/block.md) |
| [`kernel/e820.c`](../../../kernel/e820.c) | [kernel/e820.c](kernel.e820.c.md) | [Memory](../modules/memory.md) |
| [`kernel/e820.h`](../../../kernel/e820.h) | [kernel/e820.h](kernel.e820.h.md) | [Memory](../modules/memory.md) |
| [`kernel/elf.c`](../../../kernel/elf.c) | [kernel/elf.c](kernel.elf.c.md) | [Memory](../modules/memory.md) |
| [`kernel/elf.h`](../../../kernel/elf.h) | [kernel/elf.h](kernel.elf.h.md) | [Memory](../modules/memory.md) |
| [`kernel/fb.c`](../../../kernel/fb.c) | [kernel/fb.c](kernel.fb.c.md) | [Console](../modules/console.md) |
| [`kernel/fb.h`](../../../kernel/fb.h) | [kernel/fb.h](kernel.fb.h.md) | [Console](../modules/console.md) |
| [`kernel/fs.c`](../../../kernel/fs.c) | [kernel/fs.c](kernel.fs.c.md) | [FS](../modules/fs.md) |
| [`kernel/fs.h`](../../../kernel/fs.h) | [kernel/fs.h](kernel.fs.h.md) | [FS](../modules/fs.md) |
| [`kernel/heap.c`](../../../kernel/heap.c) | [kernel/heap.c](kernel.heap.c.md) | [Memory](../modules/memory.md) |
| [`kernel/heap.h`](../../../kernel/heap.h) | [kernel/heap.h](kernel.heap.h.md) | [Memory](../modules/memory.md) |
| [`kernel/idt.c`](../../../kernel/idt.c) | [kernel/idt.c](kernel.idt.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/idt.h`](../../../kernel/idt.h) | [kernel/idt.h](kernel.idt.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/io.h`](../../../kernel/io.h) | [kernel/io.h](kernel.io.h.md) | [Console](../modules/console.md) |
| [`kernel/isr.asm`](../../../kernel/isr.asm) | [kernel/isr.asm](kernel.isr.asm.md) | [Tasks](../modules/tasks.md) |
| [`kernel/kbd.c`](../../../kernel/kbd.c) | [kernel/kbd.c](kernel.kbd.c.md) | [Console](../modules/console.md) |
| [`kernel/kbd.h`](../../../kernel/kbd.h) | [kernel/kbd.h](kernel.kbd.h.md) | [Console](../modules/console.md) |
| [`kernel/kmain.c`](../../../kernel/kmain.c) | [kernel/kmain.c](kernel.kmain.c.md) | [Boot](../modules/boot.md) |
| [`kernel/linker.ld`](../../../kernel/linker.ld) | [kernel/linker.ld](kernel.linker.ld.md) | [Memory](../modules/memory.md) |
| [`kernel/mouse.c`](../../../kernel/mouse.c) | [kernel/mouse.c](kernel.mouse.c.md) | [Console](../modules/console.md) |
| [`kernel/mouse.h`](../../../kernel/mouse.h) | [kernel/mouse.h](kernel.mouse.h.md) | [Console](../modules/console.md) |
| [`kernel/pack.asm`](../../../kernel/pack.asm) | [kernel/pack.asm](kernel.pack.asm.md) | [Memory](../modules/memory.md) |
| [`kernel/pic.c`](../../../kernel/pic.c) | [kernel/pic.c](kernel.pic.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/pic.h`](../../../kernel/pic.h) | [kernel/pic.h](kernel.pic.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/pit.c`](../../../kernel/pit.c) | [kernel/pit.c](kernel.pit.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/pit.h`](../../../kernel/pit.h) | [kernel/pit.h](kernel.pit.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/pmm.c`](../../../kernel/pmm.c) | [kernel/pmm.c](kernel.pmm.c.md) | [Memory](../modules/memory.md) |
| [`kernel/pmm.h`](../../../kernel/pmm.h) | [kernel/pmm.h](kernel.pmm.h.md) | [Memory](../modules/memory.md) |
| [`kernel/rtc.c`](../../../kernel/rtc.c) | [kernel/rtc.c](kernel.rtc.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/rtc.h`](../../../kernel/rtc.h) | [kernel/rtc.h](kernel.rtc.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/sched.c`](../../../kernel/sched.c) | [kernel/sched.c](kernel.sched.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/sched.h`](../../../kernel/sched.h) | [kernel/sched.h](kernel.sched.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/serial.c`](../../../kernel/serial.c) | [kernel/serial.c](kernel.serial.c.md) | [Console](../modules/console.md) |
| [`kernel/serial.h`](../../../kernel/serial.h) | [kernel/serial.h](kernel.serial.h.md) | [Console](../modules/console.md) |
| [`kernel/user.c`](../../../kernel/user.c) | [kernel/user.c](kernel.user.c.md) | [Tasks](../modules/tasks.md) |
| [`kernel/user.h`](../../../kernel/user.h) | [kernel/user.h](kernel.user.h.md) | [Tasks](../modules/tasks.md) |
| [`kernel/vga.c`](../../../kernel/vga.c) | [kernel/vga.c](kernel.vga.c.md) | [Console](../modules/console.md) |
| [`kernel/vga.h`](../../../kernel/vga.h) | [kernel/vga.h](kernel.vga.h.md) | [Console](../modules/console.md) |
| [`kernel/virtio.c`](../../../kernel/virtio.c) | [kernel/virtio.c](kernel.virtio.c.md) | [Block](../modules/block.md) |
| [`kernel/virtio.h`](../../../kernel/virtio.h) | [kernel/virtio.h](kernel.virtio.h.md) | [Block](../modules/block.md) |
| [`kernel/vmm.c`](../../../kernel/vmm.c) | [kernel/vmm.c](kernel.vmm.c.md) | [Memory](../modules/memory.md) |
| [`kernel/vmm.h`](../../../kernel/vmm.h) | [kernel/vmm.h](kernel.vmm.h.md) | [Memory](../modules/memory.md) |
| [`CMakeLists.txt`](../../../CMakeLists.txt) | [CMakeLists.txt](CMakeLists.txt.md) | [CMake](../build/cmake.md) |
| [`cmake/embed_bin.cmake`](../../../cmake/embed_bin.cmake) | [cmake/embed_bin.cmake](cmake.embed_bin.cmake.md) | [CMake](../build/cmake.md) |
| [`cmake/concat.cmake`](../../../cmake/concat.cmake) | [cmake/concat.cmake](cmake.concat.cmake.md) | [CMake](../build/cmake.md) |
| [`user/user.ld`](../../../user/user.ld) | [user/user.ld](user.user.ld.md) | [Userland](../modules/userland.md) |
| [`user/crt0.asm`](../../../user/crt0.asm) | [user/crt0.asm](user.crt0.asm.md) | [Userland](../modules/userland.md) |
| [`user/crtstart.asm`](../../../user/crtstart.asm) | [user/crtstart.asm](user.crtstart.asm.md) | [Userland](../modules/userland.md) |
| [`user/crt.asm`](../../../user/crt.asm) | [user/crt.asm](user.crt.asm.md) | [Userland](../modules/userland.md) |
| [`user/fd.asm`](../../../user/fd.asm) | [user/fd.asm](user.fd.asm.md) | [Userland](../modules/userland.md) |
| [`user/string.c`](../../../user/string.c) | [user/string.c](user.string.c.md) | [Userland](../modules/userland.md) |
| [`user/printf.c`](../../../user/printf.c) | [user/printf.c](user.printf.c.md) | [Userland](../modules/userland.md) |
| [`user/malloc.c`](../../../user/malloc.c) | [user/malloc.c](user.malloc.c.md) | [Userland](../modules/userland.md) |
| [`user/font.c`](../../../user/font.c) | [user/font.c](user.font.c.md) | [Userland](../modules/userland.md) |
| [`user/font.h`](../../../user/font.h) | [user/font.h](user.font.h.md) | [Userland](../modules/userland.md) |
| [`user/init.c`](../../../user/init.c) | [user/init.c](user.init.c.md) | [Userland](../modules/userland.md) |
| [`user/sh.c`](../../../user/sh.c) | [user/sh.c](user.sh.c.md) | [Userland](../modules/userland.md) |
| [`user/hi.c`](../../../user/hi.c) | [user/hi.c](user.hi.c.md) | [Userland](../modules/userland.md) |
| [`user/bss.c`](../../../user/bss.c) | [user/bss.c](user.bss.c.md) | [Userland](../modules/userland.md) |
| [`user/fbhello.c`](../../../user/fbhello.c) | [user/fbhello.c](user.fbhello.c.md) | [Userland](../modules/userland.md) |
| [`user/malloctest.c`](../../../user/malloctest.c) | [user/malloctest.c](user.malloctest.c.md) | [Userland](../modules/userland.md) |

## NASM test ELFs (no note)

| Path |
| --- |
| [`user/a.asm`](../../../user/a.asm) |
| [`user/b.asm`](../../../user/b.asm) |
| [`user/brktest.asm`](../../../user/brktest.asm) |
| [`user/c.asm`](../../../user/c.asm) |
| [`user/cat.asm`](../../../user/cat.asm) |
| [`user/catch.asm`](../../../user/catch.asm) |
| [`user/cowtest.asm`](../../../user/cowtest.asm) |
| [`user/date.asm`](../../../user/date.asm) |
| [`user/dup2test.asm`](../../../user/dup2test.asm) |
| [`user/echo.asm`](../../../user/echo.asm) |
| [`user/exectest.asm`](../../../user/exectest.asm) |
| [`user/fat.asm`](../../../user/fat.asm) |
| [`user/fb.asm`](../../../user/fb.asm) |
| [`user/fbclear.asm`](../../../user/fbclear.asm) |
| [`user/fbinfo.asm`](../../../user/fbinfo.asm) |
| [`user/fbtest.asm`](../../../user/fbtest.asm) |
| [`user/fdtest.asm`](../../../user/fdtest.asm) |
| [`user/forktest.asm`](../../../user/forktest.asm) |
| [`user/kill.asm`](../../../user/kill.asm) |
| [`user/ls.asm`](../../../user/ls.asm) |
| [`user/mmapfile.asm`](../../../user/mmapfile.asm) |
| [`user/mmaptest.asm`](../../../user/mmaptest.asm) |
| [`user/mprottest.asm`](../../../user/mprottest.asm) |
| [`user/munmaptest.asm`](../../../user/munmaptest.asm) |
| [`user/opentest.asm`](../../../user/opentest.asm) |
| [`user/pipefork.asm`](../../../user/pipefork.asm) |
| [`user/pipetest.asm`](../../../user/pipetest.asm) |
| [`user/readtest.asm`](../../../user/readtest.asm) |
| [`user/sleeper.asm`](../../../user/sleeper.asm) |
| [`user/stat.asm`](../../../user/stat.asm) |
| [`user/status.asm`](../../../user/status.asm) |
| [`user/uptime.asm`](../../../user/uptime.asm) |
| [`user/waiter.asm`](../../../user/waiter.asm) |
| [`user/waitpid.asm`](../../../user/waitpid.asm) |
