---
tags: [module]
---

# Userland

Freestanding C and NASM programs on the FAT volume. Link address `0x400000`.

## Capabilities

- `init` reaps and restarts `sh`.
- `sh`: nested `|`, one `<` `>` or `>>`, trailing `&`, builtins `cd`/`pwd`.
- CRT: `_start`, `write`/`exit` stubs, `printf`/`snprintf` (`%s` `%d` `%x`), `malloc`/`free` on `brk`.
- Tests packed as ELFs (`hi`, `bss`, `mmaptest`, `catch`, `fbhello`, …).

## How it is implemented

- [[user.user.ld]] — `PT_LOAD` text+data, BSS in the data segment.
- [[user.crt0.asm]] / [[user.crtstart.asm]] / [[user.crt.asm]] / [[user.fd.asm]]
- [[user.string.c]] · [[user.printf.c]] · [[user.malloc.c]] · [[user.font.c]]
- [[user.init.c]] · [[user.sh.c]] · [[user.hi.c]] · [[user.bss.c]] · [[user.fbhello.c]] · [[user.malloctest.c]]

NASM tests are listed in [[files/_Index]] without a note each.

## See also

- [[features/Shell]]
- [[features/ELF and COW]]
- [[build/CMake]]
