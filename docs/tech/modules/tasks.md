# Tasks

GDT/TSS, IDT, `int 0x30`, 16-slot scheduler, eight fds, pipes, pending signals.

## Capabilities

- Private CR3 per task; COW user pages on `fork`.
- `TASK_MAX` 16. `ps` shows id, name, RUN/SLEEP/WAIT, PIT ticks.
- Fds 0/1/2 start as console (kbd / VGA / VGA err, mirrored to COM1).
- Pipes: one-page kernel ring. `wait`/`waitpid`, `kill`, `sigaction` (SIGINT/SIGTERM, SA_RESETHAND).
- `brk`, anonymous and file-backed `mmap`, `munmap`, `mprotect`.

## How it is implemented

- [kernel/user.c](../files/kernel.user.c.md) — syscall switch, `user_run` / pipeline, GDT helpers used from here.
- [kernel/sched.c](../files/kernel.sched.c.md) — slots, fds, pipes, kill/signal delivery on return to user.
- [kernel/idt.c](../files/kernel.idt.c.md) + [kernel/isr.asm](../files/kernel.isr.asm.md) — vectors; syscall `0x30`.
- [kernel/pic.c](../files/kernel.pic.c.md) / [kernel/pit.c](../files/kernel.pit.c.md) — IRQ remap; 100 Hz (`1193182/100`).

Syscall table: [Syscalls](../features/syscalls.md). ELF + COW: [ELF and COW](../features/elf-and-cow.md).

## Public headers

- [kernel/user.h](../files/kernel.user.h.md)
- [kernel/sched.h](../files/kernel.sched.h.md)
- [kernel/idt.h](../files/kernel.idt.h.md)
- [kernel/elf.h](../files/kernel.elf.h.md)

## See also

- [Shell](../features/shell.md)
- [Userland](userland.md)
