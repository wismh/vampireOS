---
tags: [module]
---

# Tasks

GDT/TSS, IDT, `int 0x30`, 16-slot scheduler, eight fds, pipes, pending signals.

## Capabilities

- Private CR3 per task; COW user pages on `fork`.
- `TASK_MAX` 16. `ps` shows id, name, RUN/SLEEP/WAIT, PIT ticks.
- Fds 0/1/2 start as console (kbd / VGA / VGA err, mirrored to COM1).
- Pipes: one-page kernel ring. `wait`/`waitpid`, `kill`, `sigaction` (SIGINT/SIGTERM, SA_RESETHAND).
- `brk`, anonymous and file-backed `mmap`, `munmap`, `mprotect`.

## How it is implemented

- [[kernel.user.c]] — syscall switch, `user_run` / pipeline, GDT helpers used from here.
- [[kernel.sched.c]] — slots, fds, pipes, kill/signal delivery on return to user.
- [[kernel.idt.c]] + [[kernel.isr.asm]] — vectors; syscall `0x30`.
- [[kernel.pic.c]] / [[kernel.pit.c]] — IRQ remap; 100 Hz (`1193182/100`).

Syscall table: [[features/Syscalls]]. ELF + COW: [[features/ELF and COW]].

## Public headers

- [[kernel.user.h]]
- [[kernel.sched.h]]
- [[kernel.idt.h]]
- [[kernel.elf.h]]

## See also

- [[features/Shell]]
- [[modules/Userland]]
