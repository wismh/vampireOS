---
tags: [feature]
---

# Syscalls

Ring 3 enters the kernel with `int 0x30`. Number in `rax`. Args in `rdi` `rsi` `rdx` as each call documents. Implemented in [[kernel.user.c]].

| # | Name | Notes |
| --- | --- | --- |
| 1 | write | User string → VGA, or fd + buf + len for files/pipes/console |
| 2 | exit | 8-bit code in `rdi` |
| 3 | yield | |
| 4 | sleep | PIT ticks |
| 5 | wait | `rdi` 0 any child, else pid; `rax` code or -1 |
| 6 | open | `rsi` 1 create, 2 create+append |
| 7 | close | |
| 8 | read | Console fd 0 may block for a line |
| 9 | readdir | |
| 10 | exec | Same slot |
| 11 | pipe | Two ints: read, write |
| 12 | brk | Heap from `0x402000` |
| 13 | fork | COW; child `rax` 0 |
| 14 | dup2 | |
| 15 | lseek | SEEK_SET |
| 16 | stat | Packed size, first cluster, is-dir |
| 17 | kill | Pending SIGTERM; exit on return to user |
| 18 | mmap | `rsi` length rounded to pages; `rdx` file fd copies file |
| 19 | uptime | `idt_ticks() / 100` |
| 20 | chdir | |
| 21 | getcwd | `/` or `/sub` |
| 22 | sync | FATs + cwd + `bflush` |
| 23 | munmap | |
| 24 | fbinfo | width, height, pitch, phys |
| 25 | fbpix | rect on shadow |
| 26 | fbpresent | Shadow → LFB |
| 27 | sigaction | signo + handler VA or 0; SA_RESETHAND |
| 28 | mprotect | PROT_READ or READ\|WRITE |
| 29 | date | `YYYY-MM-DD HH:MM:SS` CMOS snapshot + PIT seconds |

Unknown `rax` fails. Kernel pointers from ring 3 fail copies.

See [[modules/Tasks]], [[kernel.user.h]].
