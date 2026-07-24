# April 2026 – July 2026 (closed at vos-150)

This quarter closed after `vos-149`. Vampire OS then: BIOS MBR, FAT12 (1024 data clusters, four FAT sectors, VFAT LFN, hard links) on VirtIO-blk / AHCI / ATA, private CR3 + COW fork, `brk` / `mmap` / `munmap` / `mprotect`, eight fds, user `init` → `sh` with `<` `>` `>>` nested `|` `&` `cd` / `pwd`, VBE 640×480×32, COM1, one virtio-net UDP, CMOS `date` plus PIT, ELF BSS + extra PT_LOAD. History of vos-98 … vos-149 stays below. Unfinished July work is parked. August–October 2026 is a short next-quarter list, **not started**.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Disk pad is still 128 KiB (`KERNEL_SECTORS` 256, `PART_LBA` 273, image 1307 sectors); `kernel.raw.bin` is ~130724 of 131072 bytes (tight). PMM `KERNEL_SIZE` is 304 sectors so virtio-net / ELF-loader `.text` does not push `.bss` onto the bitmap. Bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when the on-disk image needs more than 256 sectors. No full libc, no second language — stay on the clang/nasm/lld flags in `CMakeLists.txt`. Grow freestanding helpers only.

## Now

Truth after vos-91 … vos-137. This is what boots, not what might.

- Shell: `sh` parses nested `|`, one `<`, `>`, or `>>`, and a trailing `&`. New tasks start with fds 0/1/2 on the console (keyboard / VGA / VGA err, mirrored to COM1). `cd` / `pwd` are builtins. Kernel `mv` rewrites both parent dirents so a file can change directories. Kernel `trunc name N` sets size and frees trailing FAT clusters. `$` `cp hello sub/hello` copies into an existing parent. Proof: `cat hello | cat | cat` prints `blood`; `hi > out` then `cat out` prints `hi 42`; `cat < hello` prints `blood`; `hi >> out` twice then `cat out` prints `hi 42hi 42`; `sleeper &` returns `$` while that sleeper stays in `ps`; `cd sub` then `ls` lists `sub`; `pwd` prints `/sub`; `mv sub/note dusk` leaves the clusters put; `fill note 600` then `trunc note 5` / `cat note` shows five bytes; `$ cat sub/hello` prints `blood`.
- FAT12: four FAT sectors, 1024 data clusters, 8.3 names plus VFAT LFN (`ls` / `open` / `cat longname.txt`; `put` / `cp` / `>` of a longer name writes LFN + 8.3 alias). Hard links: `$` `ln hello dual` adds a second dirent on the same first cluster; `$ rm hello` leaves `$ cat dual` printing `blood`; a last `$ rm dual` frees the chain (`$ cat dual` prints `?`). `$` `sync` rewrites both FAT copies and the cwd directory, then flushes the active backend. The MBR holds a type `0x01` FAT12 partition at LBA **273** (`PART_LBA` with the 256-sector kernel pad); boot prints `part 273`. `$` `cat bad` (packed first cluster `0x0F00`, past the 1307-sector image) prints `?` and `$` returns. Write barrier: VirtIO `BLK_T_FLUSH` (first flush prints `virt flush`), else AHCI FLUSH CACHE `E7h` (`ahci flush`), else ATA `E7h`. `$` `cat hello` still prints `blood`.
- mmap / heap / printf: file-backed `mmap` (`run mmapfile` opens `hello` and writes `blood` without `read`); `munmap` (`run munmaptest` writes `ok`, a `write` of that VA returns `-1`, then a store `#PF`-kills); mmap N pages (`run mmaptest` maps three pages at `0x500000` and writes `mmap` from the last); `mprotect` (`run mprottest` writes `ro`, then a store `#PF`-kills). `malloc` / `free` on `brk` (`run malloctest` prints `ok`). Freestanding `printf` / `snprintf` (`%s` `%d` `%x`; `run hi` prints `hi 42`).
- FB: stage 2 sets VBE 640×480×32 when the BIOS offers it (else 800×600×32 or 24 bpp); the kernel maps the LFB in the HHDM and paints a bitmap-font `Vampire OS` banner. `run fbinfo` prints `640x480` (or whatever linear mode actually booted). `run fbtest` / `run fbhello` paint via `SYS_FBPIX`; `SYS_FBPRESENT` copies a PMM shadow onto the scanout. When VBE is live the kernel blits `$` plus the line buffer onto the LFB. IRQ12 PS/2 mouse: a left click prints `x,y` once on VGA and on the LFB. `run fbclear` fills the shadow; the kernel puts `$` back and one present shows fill plus `$`. 80×25 text VGA stays if the mode set fails.
- Block: AHCI + VirtIO-blk + ATA. Boot PCI-scans virtio-blk (`0x1AF4` / `0x1001` or `0x1042`) and prints `virt 55aa`; AHCI (class `0x0106`) prints `ahci 55aa`. FAT `bread`/`bwrite` go through a block table; the active volume is VirtIO → AHCI → ATA (first VirtIO write prints `virt wr`, else `ahci wr`). `$` `devs` lists probed names (`virt ahci ata`). BIOS still boots from IDE; QEMU attaches the same image to AHCI and `-device virtio-blk-pci` without `snapshot=on` (`file.locking=off`, `cache=writethrough`).
- Tasks: ELF at `0x400000`, stack `0x401000`, heap from `0x402000`. COW fork. `TASK_MAX` 16. Eight fds. `$` `sleeper &` repeated fills slots past 8; `ps` lists those live ids and a per-task PIT tick count (`0 sh RUN 12`). `$` `kill <id>` sets a pending SIGTERM bit; the task is woken if blocked and exits on return to user. `sigaction` (syscall 27): `run catch` plus Ctrl+C jumps to that VA once (SA_RESETHAND) and prints `caught`. Default SIGINT/SIGTERM still terminate.
- Serial: COM1 (`0x3F8`, 115200 8N1) mirrors `vga_putc` and console `write` fd 1/2. Stage 2 programs the 16550 in real mode and prints `boot` before VBE and `kmain`. QEMU `-serial stdio` shows `boot` then `$`.
- Net: virtio-net (`0x1AF4` / `0x1000` or `0x1041`) prints `net 52:54:00:12:34:56`, then one Ethernet+IPv4+UDP `hi` to `10.0.2.2:5555` (dest MAC `52:55:0a:00:02:02`) and `udp sent`. Serial log is `boot` / `net 52:54:00:12:34:56` / `udp sent` / `$`. No DHCP, TCP, ARP stack, RX queue, or sockets.
- Time: `$` `date` (syscall 29) prints `YYYY-MM-DD HH:MM:SS` from a boot CMOS snapshot (ports `0x70`/`0x71`) plus PIT seconds so a later `date` after `sleep` has advanced without re-reading CMOS. `uptime` is PIT-only (`idt_ticks() / 100`). PIT 100 Hz from divisor `1193182/100` (~99.998 Hz); no NTP. QEMU `-rtc base=2026-07-22T18:50:00`.
- ELF: the loader honors BSS (`p_filesz < p_memsz`) and extra `PT_LOAD`. `run bss` prints `bss ok`.
- Pad: still `KERNEL_SECTORS` **256**, `PART_LBA` **273**, image **1307** sectors, `kernel.raw.bin` ~130724/131072 (tight). PMM `KERNEL_SIZE` 304.
- Syscalls through `int 0x30`: write, exit, yield, sleep, wait/waitpid, open (rsi 1 creates, rsi 2 creates and appends), close, read, readdir, exec, pipe, brk, fork, dup2, lseek, stat, kill, mmap (rsi length rounded up to pages, rdx a file fd copies that file into the mapping), munmap, mprotect (28; rdi VA, rsi length, rdx PROT_READ or PROT_READ|PROT_WRITE), fbinfo, fbpix, fbpresent, sigaction (27; rdi signo, rsi handler VA or 0), uptime, date (29; rdi buf, rsi max; copies `YYYY-MM-DD HH:MM:SS` from a CMOS snapshot plus PIT seconds), chdir, getcwd, sync.
- Userland C: `hi`, `sh`, `init`, `fbhello`, `bss`, CRT stubs, 8×8 font helper, `memcpy` / `strlen` / `strcmp`, freestanding `printf` / `snprintf` (`%s` `%d` `%x`; `write` fd 1), `malloc` / `free` on `brk`. Packed `catch` installs SIGINT and sleeps.

## Parked (did not ship this quarter)

Do not pretend these landed. Some reappear below as next-quarter slices; they are still **not started**.

- Second CPU / AP spin — the last sprint chose ELF BSS (`vos-149`) instead of LAPIC/SIPI; no `cpu1`.
- TCP, DHCP, ARP stack, sockets.
- UEFI boot path, USB, audio, VirtIO-GPU.
- Full POSIX signals, demand paging from file, ASLR, dynamic ELF linking.
- Real libc stdio / `FILE*`.
- SMP scheduling, FAT32, block journals.
- Job control (`fg` / `bg` / `jobs`).

---

# Sprint 1 — shell that feels like a shell

`sh` can `exec` one name, parse nested `|`, apply one `<`, `>`, or `>>`, background a line with trailing `&`, and `cd` / `pwd` as builtins. Kernel `mv` rewrites both parent dirents so a file can change directories. Kernel `trunc` sets size and frees trailing clusters. Kernel fallback still has two-way `|` and the same redirects.

## Week 1 — redirects and nested pipes

1. **Shell `<` and `>`** — done: `sh` (and kernel fallback) parse one `<` or `>`. `cat < hello` opens `hello` on fd 0; `hi > out` creates/overwrites on fd 1. `hi > out` then `cat out` shows `hi`.
2. **Nested `|` in `sh`** — done: `sh` parses `a | b | c` with `pipe`/`fork`/`dup2`/`exec` (no kernel `|` required for the happy path). `cat hello | cat | cat` prints `blood`. Left-to-right; no `&` yet.

## Week 2 — console and append

3. **Console fds 0/1/2** — done: every new task starts with fd 0/1/2 bound to the kernel console (keyboard / VGA). Legacy string-only `write` path stays for old NASM tests; C programs and `echo` / `cat` write fd 1 (or fd 2 for `cat` fail). `run echo hi` prints `hi`; `hi > out` and nested pipes still work.
4. **`>>` append** — done: `sh` (and kernel fallback) parse `>>` as append (`open` flag 2 seeks to size). `hi >> out` twice then `cat out` shows `hihi`. `hi > out` still overwrites.

## Week 3 — background and `cd` in `sh`

5. **Background `&`** — done: `sleeper &` returns the `$` prompt while the sleeper stays in `ps`. No job control; `wait` from `sh` optional later.
6. **`cd` / `pwd` in `sh`** — done: builtins change the shell task’s cwd (already per-task). `$` `cd sub` then `ls` lists `sub`. Kernel `cd` remains for the fallback prompt.

## Week 4 — volume polish left from spring

7. **Cross-directory `mv`** — done: move a file between directories by rewriting both parents; clusters stay put. `mv sub/note dusk` works; overwrite existing name prints `?`.
8. **`truncate`** — done: kernel `trunc name N` (also at `$`, like `mv`) sets size and frees trailing FAT clusters when shrinking. `fill note 600` then `trunc note 5` / `cat note` shows five bytes.

---

# Sprint 2 — a bigger, friendlier volume

8.3 names and FAT12 ceilings start to hurt once userland grows.

## Week 1 — long names

9. **LFN read** — done: recognize VFAT long-name entries enough to `open` / `ls` a hand-packed `longname.txt` on the volume. Creating LFNs can wait one slice.
10. **LFN create** — done: `put` / `cp` / `>` of a long name writes LFN + 8.3 alias. `ls` shows the long form. ASCII only.

## Week 2 — size and sync

11. **FAT16 (or more FAT12 sectors)** — done: stayed FAT12 (FAT16 is a bigger BPB/entry-width change). `FAT_SEC_PER_FAT` 4; `FAT_DATA_CLUSTERS` 1024 so a FAT12 entry can sit past two sectors. `fill note 2048` still lists and `cat`s.
12. **`sync` / flush** — done: `$` `sync` (kernel intercept like `trunc` / `mv`; syscall 22) rewrites both FAT copies and the cwd directory, then ATA FLUSH CACHE (`E7h`). After `$ hi > keep.txt` then `$ sync`, a cold QEMU restart still lists and `cat`s `keep.txt` (`cache=writethrough` on the IDE drive).

## Week 3 — directories and links

13. **`mkdir -p` / nested `cp`** — done: `$` `cp hello sub/hello` (kernel intercept like `mv`) copies into an existing parent. `$ mkdir sub` then `$ cp hello sub/hello` then `$ cat sub/hello` prints `blood`; `$ ls sub` lists `hello`. Dest parent must already exist (`mkdir -p` waits).
14. **Hard links** — done: `$` `ln hello dual` adds a second dirent on the same first cluster (no new clusters); `$ cat dual` prints `blood`. `$ rm hello` leaves `dual` readable; a last `$ rm dual` frees the chain (`$ cat dual` prints `?`). `$ ln sub dual` of a directory prints `?`. FAT12 has no inode; `rm` scans other dirents in that directory for the same first cluster.

## Week 4 — file-backed memory

15. **`mmap` a file** — done: `SYS_MMAP` with `rdx` a file fd maps a private page filled from that open file. `run mmapfile` opens `hello`, maps it at `0x500000`, and writes `blood` from the mapping without `read`. Anonymous `rdx` 0 (or a non-file fd) still maps a blank page for `mmaptest`.
16. **`munmap`** — done: `SYS_MUNMAP` (23, rdi=VA, rsi=length) unmaps one anonymous or file-backed mmap page and frees the frame. `run munmaptest` maps at `0x500000`, unmaps, writes `ok`; a `write` of that VA returns `-1`; `mem` climbs; a store after sleep `#PF`-kills the task. `mmaptest` / `mmapfile` still map if they do not `munmap`.

---

# Sprint 3 — leave the 80×25 grid

Text VGA is the only face. Give the kernel a linear framebuffer without abandoning the shell.

## Week 1 — mode set

17. **Framebuffer boot** — done: stage 2 sets a VBE linear mode (640×480×32 preferred). The kernel maps the LFB through the HHDM and paints a bitmap-font `Vampire OS` banner. 80×25 text VGA remains if the mode set fails so `$` still types.
18. **FB info syscall** — done: `SYS_FBINFO` (24) fills a user `{width, height, pitch, phys}` packed-int struct from the boot VBE block. `run fbinfo` prints `640x480` (or whatever mode actually booted).

## Week 2 — drawing

19. **User pixels** — done: `SYS_FBPIX` (25) fills a user `{x, y, w, h, color}` packed-int rectangle on the HHDM LFB. `run fbtest` paints a bar; `$` still types on a text overlay row.
20. **Bitmap font in userland** — done: tiny C helper (`user/font.c`) draws an 8×8 ASCII subset onto the FB via `SYS_FBPIX`; `run fbhello` draws `hello` without the kernel banner path.

## Week 3 — input on FB

21. **Cursor / prompt on FB** — done: `$` prompt and typed characters render on the linear framebuffer when FB mode is active (kernel bitmap font, bottom row, redrawn from the line buffer on each key). Kernel fallback `kbd>` does the same; 80×25 text VGA remains if the mode set fails.
22. **Mouse (optional lite)** — done: enable the i8042 AUX port and IRQ12, parse standard 3-byte PS/2 packets, keep pointer `x,y`; a left click prints `x,y` once on VGA row 2 and on the LFB. `$` / `fbhello` still work.

## Week 4 — double buffer and tear-free clear

23. **Clear / color** — done: `run fbclear` fills the LFB with a solid color via `SYS_FBPIX` of the full rect; the kernel redraws `$` on the overlay row so the next prompt is still visible.
24. **Swap / present** — done: a PMM shadow (not BSS — 640×480×32 would push `KERNEL_SIZE` past the 2 MiB identity window) takes `SYS_FBPIX` paints; `SYS_FBPRESENT` (26) copies it onto the LFB. `run fbclear` fills the shadow, the kernel redraws `$`, and one present shows the solid fill plus `$`.

---

# Sprint 4 — disks that are not only ATA PIO

Same image; better backends for QEMU and real hardware later.

## Week 1 — AHCI

25. **AHCI identify / read** — done: PCI scan for class 0x0106, map ABAR, set GHC.AE, IDENTIFY then READ DMA EXT LBA 0 from one implemented port into a PMM DMA page (HHDM CPU view, physical address for the HBA). Boot prints `ahci 55aa` on VGA and the LFB. FAT stays on ATA PIO (`if=ide`); QEMU adds `-device ahci,id=ahci` plus that image on `ahci.0` (`snapshot=on`, `file.locking=off`).
26. **AHCI write** — done: FAT `ata_read`/`ata_write` go through AHCI DMA (`READ/WRITE DMA EXT`) when the controller probed, else ATA PIO. First AHCI volume write prints `ahci wr`. QEMU drops `snapshot=on` on the AHCI view of `vampire.img` (`file.locking=off`, `cache=writethrough`); BIOS still boots IDE. `$ hi > ahci.txt` then `$ sync`, cold restart, `$ cat ahci.txt` still prints `hi`.

## Week 2 — VirtIO-blk

27. **VirtIO-blk read** — done: PCI scan for virtio-blk (`0x1AF4` / `0x1001` or `0x1042`), legacy I/O or virtio 1.0 MMIO virtqueue, READ LBA 0, boot prints `virt 55aa`. FAT stays on AHCI/ATA. QEMU adds `-device virtio-blk-pci` on the same image (`file.locking=off`).
28. **VirtIO-blk write** — done: FAT `ata_read`/`ata_write` prefer VirtIO → AHCI → ATA. First VirtIO volume write prints `virt wr`. `$ hi > virt.txt` then `$ sync`, cold restart, `$ cat virt.txt` still prints `hi`.

## Week 3 — block layer

29. **Block device table** — done: FAT `bread`/`bwrite` through a small name+read/write table. ATA, AHCI, and VirtIO register when they probe. Active FS device stays VirtIO → AHCI → ATA. `$` `devs` lists probed names (`virt ahci ata`). `help` lists `devs`.
30. **Partition read** — done: MBR partition type `0x01` (FAT12) at start LBA 273 (`PART_LBA` = kernel LBA + `KERNEL_SECTORS`). VBR/BPB lives there; reserved FAT sectors are 1. `bread`/`bwrite` add that start so `$` `ls` / `cat hello` work off the partition. Boot prints `part 273`. BIOS MBR still loads stage 2 from LBA 1.

## Week 4 — reliability

31. **Read errors surface** — done: `bread` returns `-1` on a timeout, device error bit, or LBA past the disk instead of spinning forever. Packed `bad` has first cluster `0x0F00` (past the volume and the 1307-sector image); `$` `cat bad` prints `?` and `$` comes back. `$` `cat hello` still prints `blood`. `open` / `read` of that path return `-1`.
32. **Write barrier** — done: `$` `sync` flushes the active FS device (VirtIO `BLK_T_FLUSH` when virtio-blk is the volume, else AHCI FLUSH CACHE `E7h` via a DMA command, else ATA `E7h`). First VirtIO flush prints `virt flush`; first AHCI flush prints `ahci flush`. After `$ hi > bar.txt` then `$ sync`, a cold restart still `cat`s `bar.txt`. `$` `cat bad` still prints `?`; `$` `cat hello` still prints `blood`.

---

# Sprint 5 — processes and signals grow up

Eight tasks and kill-as-DEAD are not enough once `sh` backgrounds work.

## Week 1 — scale

33. **`TASK_MAX` 16** — done: table is 16; scans use `TASK_MAX` / `task_count`. `sleeper &` enough times (or a sixteen-deep `fork`) still schedules; `ps` lists slots ≥ 8. Extra tasks share a VGA overlay row.
34. **`nice` / priority lite** — done: skipped `nice`; each live slot counts PIT ticks spent as current (wrapping 32-bit) and `ps` prints that column (`0 sh RUN 12`). After a pause, idle/init/sh show different counts; a second `ps` shows at least one slot grew.

## Week 2 — signals lite

35. **Pending signal bit** — done: `kill` sets a SIGTERM pending bit and an 8-bit status; return-to-user (syscall / IRQ / #PF resume) consumes it and the task exits. A sleeping `sleeper` is woken so `$ sleeper &` / `$ kill <id>` / `$ ps` drops that slot. No `sigaction`.
36. **`sigaction` stub** — done: `SYS_SIGACTION` (27, rdi=signo, rsi=handler VA or 0) installs SIGINT (2) or SIGTERM (15). Default still terminates. `run catch` plus Ctrl+C jumps to the handler once (SA_RESETHAND) and prints `caught`. No full POSIX set.

## Week 3 — memory hygiene

37. **Grow `mmap`** — done: `SYS_MMAP` honors rsi (round up to 4 KiB, at most 16 pages) at the hint VA. Overlap or OOM returns -1. `run mmaptest` maps 3 pages at `0x500000`, stores on the last page, and writes `mmap`. `SYS_MUNMAP` unmaps that many pages. `mmapfile` / `munmaptest` still work. PMM `KERNEL_SIZE` 288 so `.bss` stays below the bitmap. No mprotect.
38. **`mprotect`** — done: `SYS_MPROTECT` (28, rdi=VA, rsi=length, rdx=PROT_READ or PROT_READ|PROT_WRITE) updates user PTEs and `invlpg`s. `run mprottest` maps at `0x500000`, marks it RO, writes `ro`; a store `#PF`-kills the task. Shared pages keep COW until write (`run cowtest` still prints `parent` and `child`). `run mmaptest` still writes `mmap` from page 3. No printf/malloc.

## Week 4 — userland libc growth (freestanding)

39. **`printf` lite** — done: freestanding `printf` / `snprintf` in `user/printf.c` (`%s` `%d` `%x` only). `printf` writes fd 1 through `write`. `run hi` prints `hi 42` via `printf("hi %d", 42)`. No malloc.
40. **`malloc` on `brk`** — done: free-list allocator in `user/malloc.c` (query `brk` with rdi=0, grow to a new break). `run malloctest` allocates two blocks, writes distinct bytes, frees one, mallocs again, and prints `ok`. No sbrk games beyond existing `brk`.

---

# Sprint 6 — talk to the outside

Local disk and VGA/FB work; the machine is still mute on the wire and often silent without a window.

## Week 1 — serial

41. **Serial console** — done: kernel mirrors `vga_putc` (the `$` prompt and typed keys) and console `write` fd 1/2 to a 16550 at COM1 (`0x3F8`, 115200 8N1). COM1 RX injects into the same line buffer as PS/2. QEMU `-serial stdio` (or `-serial file:...`) shows `$`; `$` `cat hello` still prints `blood` on that log. The LFB overlay `$` still types.
42. **Early boot serial** — done: stage 2 inits the 16550 at COM1 (`0x3F8`, 115200 8N1) in real mode (`out dx,al`) and prints `boot` before VBE/`kmain`. QEMU `-serial stdio` (or `-serial file:...`) shows `boot` then `$`. Kernel console mirror stays.

## Week 2 — networking stub

43. **VirtIO-net probe** — done: PCI scan for virtio-net (`0x1AF4` / `0x1000` or modern `0x1041`), virtio 1.0 MMIO or legacy I/O device config, read MAC, boot prints `net aa:bb:cc:dd:ee:ff` on VGA, the LFB, and COM1. No TX/RX queues or packets (blk virtqueues stay on the virtio-blk device). QEMU `-device virtio-net-pci` (user netdev, MAC `52:54:00:12:34:56`) next to virtio-blk/AHCI/IDE. Serial log is `boot` / `net 52:54:00:12:34:56` / `$`. PMM `KERNEL_SIZE` 304 so the extra `.text` does not push `.bss` onto the bitmap; disk pad stays 256 sectors (`part 273`).
44. **Send one UDP** — done: TX virtqueue on virtio-net (queue 1, separate DMA from virtio-blk), one Ethernet+IPv4+UDP frame to hard-coded dest MAC `52:55:0a:00:02:02` / `10.0.2.2:5555` with payload `hi`. Boot prints `udp sent` after the MAC. QEMU `-object filter-dump` writes `build/virtio-net.pcap`. Serial log is `boot` / `net 52:54:00:12:34:56` / `udp sent` / `$`. No DHCP, TCP, or RX queue.

## Week 3 — clocks and timekeeping

45. **RTC read** — done: CMOS RTC (ports `0x70`/`0x71`; BCD or binary, 12/24h); `$` `date` prints `YYYY-MM-DD HH:MM:SS`. Syscall 29 (`SYS_DATE`) copies that string into a user buffer. QEMU `-rtc base=2026-07-21T20:25:00` (or the host clock) is the proof. Packed as `DATE`. No PIT sync.
46. **PIT + RTC sync** — done: `uptime` stays PIT seconds (`idt_ticks() / 100`); `date` latches CMOS once at boot and adds PIT seconds so it does not re-read ports `0x70`/`0x71` on each call. Wall clock does not jump backwards. PIT 100 Hz from divisor `1193182/100` (~99.998 Hz) so a second of skew vs the CMOS chip is expected; no NTP. Proof: `$` `date` then `uptime` (or `ps` ticks), pause, then `date`/`uptime` again — wall time and uptime both advance. QEMU `-rtc base=2026-07-22T18:50:00`.

## Week 4 — SMP toehold or ELF polish

47. **ELF BSS / extra PT_LOAD** — done (chose this over second CPU): the loader maps more than one `PT_LOAD` and zeros `p_memsz - p_filesz`. `user/bss.c` keeps initialized `.data` plus a `.bss` tail on a second LOAD; `run bss` prints `bss ok`. AP spin / `cpu1` did not ship — parked, listed again below.
48. **Plan checkpoint** — done: this file. “Now” matches vos-149. Unfinished July work is parked. August–October 2026 is a short list, not started. No kernel/user/boot feature code; pad stays 256.

---

# August – October 2026 — pad, a parked AP, more than one packet

Short future list. **Not started.** One `vos-N` per slice still. The 256-sector pad is tight; do the bump before any slice that will not fit. Do not start these by editing this list into “done”.

49. **Bump `KERNEL_SECTORS` and move FAT** — not started: raise the disk pad past 256 and move `PART_LBA` with it so `.text` can grow. Image size and `part N` change together. Boot still `cat hello` → `blood` off the new LBA. PMM `KERNEL_SIZE` bumps with the pad.
50. **AP spin** — not started: parked in July when vos-149 took ELF BSS. If LAPIC/SIPI is tractable, start the second QEMU CPU, park it in hlt, print `cpu1`. No SMP scheduler. If SIPI is still a mess, skip and say so — do not pretend `cpu1` printed.
51. **ARP** — not started: send or answer one ARP for the virtio-net MAC / a hard-coded IPv4. VGA/COM1 prints `arp`. No DHCP yet. The existing UDP `hi` may keep its hard-coded dest MAC until this lands.
52. **DHCP stub** — not started: one DHCP discover/request on virtio-net, print a leased IPv4 or `dhcp fail`. Needs RX. No TCP. QEMU user-net is the proof.
53. **TCP stub** — not started: a handshake or a single outbound segment the host can see (QEMU dump / `nc`). No listen socket, no `read`/`write` on a connection, no stack.
54. **FAT32 or a larger FAT12 volume** — not started: either a real FAT32 BPB or more FAT12 sectors / clusters so the volume is not stuck at 1024 clusters. `$` `ls` / `cat hello` still work. Pick one; do not dual-track.
55. **Demand paging from file** — not started: an ELF or `mmap` page that faults in from the volume instead of copying the whole file at map time. `run mmapfile` still prints `blood`. No ASLR, no dynamic linker.
56. **UEFI research** — not started: notes (and maybe a non-boot OVMF experiment) for an EFI path. BIOS MBR stays the boot that `$` uses. No USB, no “we boot UEFI now”.
57. **Plan checkpoint** — not started: rewrite “Now” from whatever actually booted; park what still did not land; draft the following quarter. Docs only.

---

## Leave for later

Still later than August–October, unless a slice above takes a bite: USB, audio, VirtIO-GPU, full POSIX signals, ASLR, dynamic ELF linking, a real libc stdio/`FILE*`, SMP scheduling beyond a parked AP, a real TCP stack, sockets, block journals.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` (add VirtIO/AHCI devices when that month needs them) with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
