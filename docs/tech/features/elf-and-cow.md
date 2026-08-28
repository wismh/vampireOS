# ELF and COW

User programs are ELF64 linked at `0x400000` ([user/user.ld](../files/user.user.ld.md)). Stack page `0x401000`. Heap `brk` from `0x402000`.

## Load

[kernel/elf.c](../files/kernel.elf.c.md) walks `PT_LOAD`:

- File bytes (`p_filesz`) copied into newly mapped user pages.
- BSS (`p_memsz > p_filesz`) zeroed.
- Extra `PT_LOAD` after the first is honored (`run bss` prints `bss ok`).

`run <name>` / `exec` allocate a cloned PML4, map those pages, set RIP/RSP. `TASK_MAX` 16. Shared link address is safe because each task has its own CR3.

## Fork

`fork` clones the page tables and marks user PTEs read-only. A write `#PF`s, the kernel copies the page, and parent/child diverge (`run cowtest`). `exec` resets the current slot’s RIP/RSP/base but keeps kstack and CR3.

Exit tears down user PTEs and table pages; kernel stack is freed when the DEAD slot is reused.

See [Memory](../modules/memory.md), [Tasks](../modules/tasks.md), [Userland](../modules/userland.md).
