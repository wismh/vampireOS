# August 2026 – January 2027

Vampire OS after `vos-89`: BIOS MBR, FAT12 on ATA PIO (1024 data clusters, four FAT sectors), private CR3 + COW fork, `brk` / `mmap`, eight fds, user `init` → `sh`, freestanding C (`hi`, CRT stubs, tiny `string.c`), `ps` / `kill` / Ctrl+C / `uptime`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Disk kernel pad is 128 KiB (`KERNEL_SECTORS` 256, `part 273`); PMM `KERNEL_SIZE` is 304 sectors so virtio-net probe `.text` growth does not push `.bss` onto the bitmap. Bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when the on-disk image needs more than 256 sectors. No full libc, no second language — stay on the clang/nasm/lld flags in `CMakeLists.txt`. Grow freestanding helpers only.

## Now

- Volume: FAT12, 1024 data clusters, four sectors per FAT (both copies). 8.3 names plus VFAT LFN (`ls` / `open` / `cat longname.txt`; `put` / `cp` / `>` of a longer name writes LFN + 8.3 alias). Boot PCI-scans for virtio-blk (vendor `0x1AF4`, device `0x1001` or modern `0x1042`), virtqueue-reads LBA 0, and prints `virt 55aa`. FAT `bread`/`bwrite` go through a block table: ATA, AHCI, and VirtIO register when they probe; the active volume stays VirtIO → AHCI → ATA (first VirtIO write prints `virt wr`, else `ahci wr`). `$` `devs` lists probed names (`virt ahci ata`). The MBR holds a type `0x01` FAT12 partition whose start LBA is after MBR + stage 2 + kernel (`PART_LBA` 273 with the 256-sector kernel pad); `bread`/`bwrite` add that LBA so the BPB is not disk LBA 0. Boot prints `part 273`. `bread` returns `-1` on a timeout, device error, or LBA past the disk (VirtIO capacity / IDENTIFY size) instead of spinning forever; `$` `cat bad` (packed first cluster `0x0F00`, past the 1307-sector image) prints `?` and `$` returns. `$` `cat hello` still prints `blood`. BIOS still boots from IDE; QEMU attaches the same image to AHCI and to `-device virtio-blk-pci` without `snapshot=on` (`file.locking=off`, `cache=writethrough`). `$` `sync` flushes the active backend (VirtIO `BLK_T_FLUSH`, else AHCI FLUSH CACHE `E7h` via DMA, else ATA `E7h`); first VirtIO flush prints `virt flush`, first AHCI flush prints `ahci flush`.
- Boot: stage 2 programs COM1 (`0x3F8`, 115200 8N1) in real mode and prints `boot` before VBE and `kmain`, then sets a VBE linear framebuffer (640×480×32 when the BIOS offers it, else 800×600×32 or 24 bpp); the kernel maps that LFB in the HHDM and paints a bitmap-font `Vampire OS` banner. 80×25 text VGA stays if the mode set fails so `$` still types. Kernel starts `init`, which `fork`/`exec`s `sh`. Prompt `$`. `sh` parses nested `|` (`cat hello | cat | cat` prints `blood`) and one `<`, `>`, or `>>` (`hi > out` then `cat out` prints `hi 42`; `cat < hello` prints `blood`; `hi >> out` twice then `cat out` prints `hi 42hi 42`). A trailing `&` backgrounds the line (`sleeper &` returns `$` while that sleeper stays in `ps`). `cd` / `pwd` are `sh` builtins (`cd sub` then `ls` lists `sub`; `pwd` prints `/sub`). Kernel `mv` rewrites both parent dirents so `mv sub/note dusk` leaves the clusters put; an existing dest name prints `?`. Kernel `trunc name N` sets the dirent size and frees trailing FAT clusters on shrink (`fill note 600` then `trunc note 5` / `cat note` shows five bytes). `$` `cp hello sub/hello` (kernel intercept like `mv`) copies into an existing parent so `$ cat sub/hello` prints `blood` and `$ ls sub` lists `hello`. `$` `ln hello dual` (kernel intercept like `cp`) adds a second dirent on the same first cluster so `$ cat dual` prints `blood`; `$ rm hello` leaves that chain until the last name (`$ cat dual` still prints `blood`); a last `$ rm dual` frees it (`$ cat dual` prints `?`). `$ ln sub dual` of a directory prints `?`. FAT12 has no inode; `rm` scans other dirents in that directory for the same first cluster before `fat_free_chain`. `$` `sync` rewrites both FAT copies and the cwd directory, then flushes the active block device (VirtIO `BLK_T_FLUSH`, else AHCI FLUSH CACHE `E7h` via a DMA command, else ATA `E7h`), so a file created then synced is still there after a cold QEMU restart (`cache=writethrough`). `$` `fill note 2048` writes a multi-KiB chain on the extra FAT map; `$` `ls` lists `note` and `$` `cat note` prints the filled bytes. `$` `ls` lists `longname.txt` from a packed VFAT LFN (8.3 alias `LONGNA~1.TXT`); `$` `cat longname.txt` prints `long`. `$` `hi > longername.txt` writes LFN + alias `LONGER~1.TXT`; `$` `ls` lists `longername.txt` and `$` `cat longername.txt` prints `hi 42`. `run hi` prints `hi 42` via freestanding `printf`. `run malloctest` allocates two blocks on the `brk` heap, frees one, and prints `ok`. `run mmaptest` maps three pages at `0x500000`, stores `mmap` on the last page, and writes it. `run mprottest` maps a page, `mprotect`s it RO, writes `ro`, and a store `#PF`-kills the task. `run mmapfile` opens `hello`, maps it, and writes `blood` from that page without `read`. `run munmaptest` maps a page, `munmap`s it, writes `ok`, and a write of that VA returns `-1`; `mem` shows the frame back; a store after a short sleep `#PF`-kills the task. `run fbinfo` fills a user `{width, height, pitch, phys}` packed-int struct and prints `640x480` (or whatever linear mode actually booted). `run fbtest` fills a packed `{x, y, w, h, color}` rectangle on the LFB; `$` still types on a text overlay row. `run fbhello` draws `hello` with a userland 8×8 bitmap font via `SYS_FBPIX` (not the kernel banner path). When VBE is live the kernel blits `$` (or fallback `kbd>`) plus the current line buffer onto the LFB with the banner font so typed input is visible on the scanout. `vga_putc` and console `write` fd 1/2 are mirrored to COM1 (16550 at `0x3F8`, 115200 8N1) so QEMU `-serial stdio` (or `-serial file:...`) shows `$` and accepts keys while the LFB owns the display. Boot PCI-scans for virtio-net (vendor `0x1AF4`, device `0x1000` or modern `0x1041`), reads the MAC from device config, sets up the TX virtqueue (blk virtqueues stay on virtio-blk), sends one hard-coded Ethernet+IPv4+UDP frame (`hi` to QEMU user-net `10.0.2.2:5555`, dest MAC `52:55:0a:00:02:02`), and prints `net aa:bb:cc:dd:ee:ff` then `udp sent` on VGA, the LFB, and COM1 so the serial log is `boot` / `net …` / `udp sent` / `$`. QEMU adds `-device virtio-net-pci` (user netdev) and `-object filter-dump` next to the existing virtio-blk/AHCI/IDE drives. IRQ12 on the i8042 AUX port streams 3-byte PS/2 mouse packets; a left click prints `x,y` once on VGA and on the LFB (a small XOR cross tracks the pointer). `run fbclear` fills a PMM shadow of the LFB with a solid color; the kernel puts `$` back on the overlay row and `SYS_FBPRESENT` (26) copies fill plus `$` onto the scanout in one blit so the next prompt is still visible. Kernel line buffer remains as fallback (`help` / `ls` / `run` / …): one `|` and the same redirects; kernel `cd` / `pwd` stay on `kbd>`.
- Tasks: ELF at `0x400000`, stack `0x401000`, heap from `0x402000`. COW fork. `TASK_MAX` 16. Eight fds. New tasks start with fd 0/1/2 on the console (keyboard / VGA / VGA err, mirrored to COM1); redirects and pipes still override those slots. `$` `sleeper &` repeated fills slots past 8; `ps` lists those live ids (and wraps on 80×25). Extra tasks share a VGA overlay row so 16 slots do not walk off the text grid. `ps` prints a per-task PIT tick count after the state (`0 sh RUN 12`); the number grows for the slot that was current across a pause. `$` `kill <id>` sets a pending SIGTERM bit (8-bit status) on a live or sleeping slot; the task is woken if blocked and exits on return to user so `ps` drops it. Kernel-shell `kill` of a slot with no CR3 still marks DEAD immediately. `$` `run catch` installs a SIGINT handler; Ctrl+C jumps to that VA (SA_RESETHAND) so VGA/FB print `caught` once. Default SIGINT/SIGTERM still terminate.
- Syscalls through `int 0x30`: write, exit, yield, sleep, wait/waitpid, open (rsi 1 creates, rsi 2 creates and appends), close, read, readdir, exec, pipe, brk, fork, dup2, lseek, stat, kill, mmap (rsi length rounded up to pages, rdx a file fd copies that file into the mapping), munmap, mprotect (28; rdi VA, rsi length, rdx PROT_READ or PROT_READ|PROT_WRITE), fbinfo, fbpix, fbpresent, sigaction (27; rdi signo, rsi handler VA or 0), uptime, chdir, getcwd, sync.
- Userland C: `hi`, `sh`, `init`, `fbhello`, CRT stubs, 8×8 font helper, `memcpy` / `strlen` / `strcmp`, freestanding `printf` / `snprintf` (`%s` `%d` `%x`; `write` fd 1), `malloc` / `free` on `brk`. `run hi` prints `hi 42`. `run malloctest` prints `ok`. Packed `catch` installs SIGINT and sleeps.
- No job control (`fg` / `bg` / `jobs`), no FAT16, no SMP, no DHCP / TCP / ARP stack, no full POSIX signals.

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

45. **RTC read** — read CMOS RTC; `date` prints `YYYY-MM-DD HH:MM:SS` (QEMU clock).
46. **PIT + RTC sync** — `uptime` stays monotonic; `date` wall-clock. Document skew; no NTP.

## Week 4 — SMP toehold or ELF polish

47. **Second CPU spin** — if LAPIC/SIPI is tractable: AP parks in a `wfi`/hlt loop and kernel prints `cpu1`. If not: **ELF BSS / multiple PT_LOAD** — loaders zero `.bss` and map two LOAD segments; a C program with a large `.bss` array prints `bss` after boot. Prefer **ELF BSS/multi-LOAD** unless SMP is already half-done.
48. **Plan checkpoint** — rewrite `docs/plan.md` “Now” from reality; park unfinished Sprint 6 items and draft the next quarter. No feature code required beyond doc truth.

---

## Leave for later

UEFI boot path, USB, audio, VirtIO-GPU, full POSIX signals, demand paging from file, ASLR, dynamic ELF linking, a real libc stdio/`FILE*`, SMP scheduling beyond a parked AP, TCP, DHCP, block journals, FAT32.

## Check

After a slice: `cmake --build build`, kernel raw size vs `KERNEL_SECTORS * 512`, QEMU `-drive format=raw,file=build/vampire.img,if=ide` (add VirtIO/AHCI devices when that month needs them) with a qcode script for the new command. Do not commit `build/`, `.tools/`, or `.cursor/skills/`.
