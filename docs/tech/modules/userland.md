# Userland

Freestanding C and NASM programs on the FAT volume. Link address `0x400000`.

## Capabilities

- `init` reaps and restarts `sh`.
- `sh`: nested `|`, one `<` `>` or `>>`, trailing `&`, builtins `cd`/`pwd`.
- CRT: `_start`, `write`/`exit` stubs, `printf`/`snprintf` (`%s` `%d` `%x`), `malloc`/`free` on `brk`.
- Tests packed as ELFs (`hi`, `bss`, `mmaptest`, `catch`, `fbhello`, …).

## How it is implemented

- [user/user.ld](../files/user.user.ld.md) — `PT_LOAD` text+data, BSS in the data segment.
- [user/crt0.asm](../files/user.crt0.asm.md) / [user/crtstart.asm](../files/user.crtstart.asm.md) / [user/crt.asm](../files/user.crt.asm.md) / [user/fd.asm](../files/user.fd.asm.md)
- [user/string.c](../files/user.string.c.md) · [user/printf.c](../files/user.printf.c.md) · [user/malloc.c](../files/user.malloc.c.md) · [user/font.c](../files/user.font.c.md)
- [user/init.c](../files/user.init.c.md) · [user/sh.c](../files/user.sh.c.md) · [user/hi.c](../files/user.hi.c.md) · [user/bss.c](../files/user.bss.c.md) · [user/fbhello.c](../files/user.fbhello.c.md) · [user/malloctest.c](../files/user.malloctest.c.md)

NASM tests are listed in [files](../files/README.md) without a note each.

## See also

- [Shell](../features/shell.md)
- [ELF and COW](../features/elf-and-cow.md)
- [CMake](../build/cmake.md)
