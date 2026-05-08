# August 2026 – January 2027

Vampire OS after `vos-89`: BIOS MBR, FAT12 on ATA PIO (1024 data clusters, four FAT sectors), private CR3 + COW fork, `brk` / `mmap`, eight fds, user `init` → `sh`, freestanding C (`hi`, CRT stubs, tiny `string.c`), `ps` / `kill` / Ctrl+C / `uptime`.

One `vos-N` slice per step. Each slice boots in QEMU and leaves a command or a line on the VGA that was impossible the day before. Kernel pad is 100 KiB (`KERNEL_SECTORS` 200); bump `KERNEL_SECTORS` and `KERNEL_SIZE` together when `.text` plus `.bss` get near the PMM bitmap again. No full libc, no second language — stay on the clang/nasm/lld flags in `CMakeLists.txt`. Grow freestanding helpers only.

## Now

- Volume: FAT12, 1024 data clusters, four sectors per FAT (both copies). 8.3 names plus VFAT LFN (`ls` / `open` / `cat longname.txt`; `put` / `cp` / `>` of a longer name writes LFN + 8.3 alias). ATA PIO only.
- Boot: kernel starts `init`, which `fork`/`exec`s `sh`. Prompt `$`. `sh` parses nested `|` (`cat hello | cat | cat` prints `blood`) and one `<`, `>`, or `>>` (`hi > out` then `cat out` prints `hi`; `cat < hello` prints `blood`; `hi >> out` twice then `cat out` prints `hihi`). A trailing `&` backgrounds the line (`sleeper &` returns `$` while that sleeper stays in `ps`). `cd` / `pwd` are `sh` builtins (`cd sub` then `ls` lists `sub`; `pwd` prints `/sub`). Kernel `mv` rewrites both parent dirents so `mv sub/note dusk` leaves the clusters put; an existing dest name prints `?`. Kernel `trunc name N` sets the dirent size and frees trailing FAT clusters on shrink (`fill note 600` then `trunc note 5` / `cat note` shows five bytes). `$` `fill note 2048` writes a multi-KiB chain on the extra FAT map; `$` `ls` lists `note` and `$` `cat note` prints the filled bytes. `$` `ls` lists `longname.txt` from a packed VFAT LFN (8.3 alias `LONGNA~1.TXT`); `$` `cat longname.txt` prints `long`. `$` `hi > longername.txt` writes LFN + alias `LONGER~1.TXT`; `$` `ls` lists `longername.txt` and `$` `cat longername.txt` prints `hi`. Kernel line buffer remains as fallback (`help` / `ls` / `run` / …): one `|` and the same redirects; kernel `cd` / `pwd` stay on `kbd>`.
- Tasks: ELF at `0x400000`, stack `0x401000`, heap from `0x402000`. COW fork. `TASK_MAX` 8. Eight fds. New tasks start with fd 0/1/2 on the console (keyboard / VGA / VGA err); redirects and pipes still override those slots.
- Syscalls through `int 0x30`: write, exit, yield, sleep, wait/waitpid, open (rsi 1 creates, rsi 2 creates and appends), close, read, readdir, exec, pipe, brk, fork, dup2, lseek, stat, kill, mmap, uptime, chdir, getcwd.
- Userland C: `hi`, `sh`, `init`, CRT stubs, `memcpy` / `strlen` / `strcmp`.
- No job control (`fg` / `bg` / `jobs`), no FAT16, no framebuffer, no AHCI/VirtIO, no serial console, no SMP, no networking, no signals beyond kill/Ctrl+C, no file-backed mmap, no `munmap`.

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
12. **`sync` / flush** — syscall or shell `sync` forces dirty FAT/dir sectors to disk (explicit ATA flush). After `put` without reboot, a cold QEMU restart still sees the file (prove with `cache=writethrough` already on; this is about not leaving cached FS state if any appears).

## Week 3 — directories and links

13. **`mkdir -p` / nested `cp`** — `cp` into an existing directory (`cp hello sub/hello`) or `mkdir` that creates parents. One behavior per slice: prefer **`cp` into a directory** first.
14. **Hard links** — `ln a b` adds a second dirent to the same cluster chain; `rm` of one name leaves the other until the last link drops. Refuse directories.

## Week 4 — file-backed memory

15. **`mmap` a file** — map a read-only (or RW) view of an open fd’s contents into user VA. `run mmapfile` maps `hello` and prints `blood` from the mapping without `read`.
16. **`munmap`** — unmap a previous anonymous or file mapping. After `munmap`, touching the VA faults cleanly (kill or `-1` path); `mem` returns frames.

---

# Sprint 3 — leave the 80×25 grid

Text VGA is the only face. Give the kernel a linear framebuffer without abandoning the shell.

## Week 1 — mode set

17. **Framebuffer boot** — stage 2 or early kernel sets a VESA/VBE (or QEMU-known) linear FB mode. Kernel draws one banner string in a bitmap font. Keep text VGA as fallback if mode set fails.
18. **FB info syscall** — user can query width/height/pitch/address (or a fixed HHDM window). `run fbinfo` prints `WxH`.

## Week 2 — drawing

19. **User pixels** — syscall or mmap’d FB: `run fbtest` fills a rectangle or plots a glyph row. Shell still usable (text overlay row or dual output to serial later).
20. **Bitmap font in userland** — tiny C helper draws ASCII onto the FB; `run fbhello` draws `hello` without the kernel banner path.

## Week 3 — input on FB

21. **Cursor / prompt on FB** — `$` prompt and typed characters render on the framebuffer when FB mode is active. Kernel fallback prompt does the same or stays on text plane.
22. **Mouse (optional lite)** — PS/2 mouse packet → pointer coordinates; click is enough to print `x,y` once. Skip if the month is slipping; replace with **scrollback** (PageUp shows prior lines) if mouse is too noisy.

## Week 4 — double buffer and tear-free clear

23. **Clear / color** — `run fbclear` fills the FB with a solid color; next prompt still visible.
24. **Swap / present** — if using a shadow buffer, one syscall presents it. If single-buffer, document that and ship a tear-aware full-frame redraw of the text row instead — still one visible improvement (stable prompt redraw after `fbclear`).

---

# Sprint 4 — disks that are not only ATA PIO

Same image; better backends for QEMU and real hardware later.

## Week 1 — AHCI

25. **AHCI identify / read** — find an AHCI controller, read one sector, print a known signature (MBR magic or a marker). Leave writes on ATA until the next slice.
26. **AHCI write** — FAT updates and `put` can go through AHCI when present, else ATA. Autodetect; no new image format.

## Week 2 — VirtIO-blk

27. **VirtIO-blk read** — PCI virtio-blk (`-device virtio-blk-pci`), read one sector, print signature. Prefer VirtIO → AHCI → ATA.
28. **VirtIO-blk write** — same preference order for FS writes. Prove `put` survives reboot under VirtIO.

## Week 3 — block layer

29. **Block device table** — one thin `bread`/`bwrite` API used by FAT; ATA/AHCI/VirtIO register underneath. Shell `devs` lists backends that probed OK.
30. **Partition read** — honor the MBR partition that holds the volume (or document “whole-disk FAT at LBA N”). `ls` still works when the FAT starts at a non-zero LBA.

## Week 4 — reliability

31. **Read errors surface** — forced bad LBA (or QEMU eject) returns `-1` from `read` / `open` paths instead of hanging. `cat` prints `?`.
32. **Write barrier** — `sync` issues cache flush commands on AHCI/VirtIO when available (ATA: flush cache if supported).

---

# Sprint 5 — processes and signals grow up

Eight tasks and kill-as-DEAD are not enough once `sh` backgrounds work.

## Week 1 — scale

33. **`TASK_MAX` 16** — raise the table; fix scans that assume 8. `fork` sixteen deep still schedules; `ps` lists them.
34. **`nice` / priority lite** — optional: sleep-less yield bias, **or** skip and do **per-task CPU tick counters** shown in `ps` (`ps` gains a tick column). Prefer **ps tick column** — visible, small.

## Week 2 — signals lite

35. **Pending signal bit** — `kill` sets a bit; before return to user, if SIGTERM-like is pending, task exits with a status. `sleeper` dies on `kill` without needing the DEAD-from-kernel-only path to be special-cased forever.
36. **`sigaction` stub** — user installs a handler address for one signal (e.g. Ctrl+C → SIGINT). Default terminates; with handler, `run catch` prints `caught` once. No full POSIX set.

## Week 3 — memory hygiene

37. **Grow `mmap`** — map N pages (length argument honored beyond one page). `run mmaptest` maps 3 pages and touches the last.
38. **`mprotect`** — change page protections (RO/RW). Writing a RO page after `mprotect` faults; COW still works with the new prot.

## Week 4 — userland libc growth (freestanding)

39. **`printf` lite** — freestanding `printf` / `snprintf` supporting `%s` `%d` `%x` only, writing through `write`. `hi` prints `hi 42` with it.
40. **`malloc` on `brk`** — tiny bump or free-list allocator in `user/malloc.c`. `run malloctest` allocates two blocks, writes, frees one, prints `ok`. No sbrk games beyond existing `brk`.

---

# Sprint 6 — talk to the outside

Local disk and VGA/FB work; the machine is still mute on the wire and often silent without a window.

## Week 1 — serial

41. **Serial console** — kernel mirrors console output to COM1; optional input from serial. QEMU `-serial stdio` shows `$` and accepts keys. Useful when FB owns the display.
42. **Early boot serial** — stage 2 or pre-`kmain` prints `boot` on COM1 so hangs before VGA are debuggable.

## Week 2 — networking stub

43. **VirtIO-net probe** — find virtio-net, read MAC, print it (`run netmac` or kernel line). No packets yet.
44. **Send one UDP** — craft a single UDP packet (fixed dest) via virtio-net TX. Host `nc -u -l` or QEMU dump sees bytes. Hard-code dest MAC/IP for the slice.

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
