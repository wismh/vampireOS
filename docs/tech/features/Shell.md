---
tags: [feature]
---

# Shell

Two prompts: user `$` (`init` → `sh`) and kernel `kbd>` fallback.

## User `sh` ([[user.sh.c]])

- Nested `|` via `pipe`/`fork`/`dup2`/`exec`.
- One `<`, `>`, or `>>` (open flag 2 appends).
- Trailing `&` — do not wait; no `fg`/`bg`/`jobs`.
- Builtins `cd` / `pwd` (syscalls 20/21).

Proof: `cat hello | cat | cat` → `blood`; `hi > out` then `cat out` → `hi 42`; `sleeper &` returns `$` while `ps` lists the sleeper.

## Kernel line ([[kernel.kbd.c]])

`help` `ls` `mem` `cat` `run` `put` `rm` `ps` `kill` `uptime` `date` `sync` `cd` `pwd` `mv` `trunc` `devs` and a two-command `|` plus one redirect. New tasks still get fds 0/1/2 on the console.

Ctrl+C posts SIGINT on the last `run` ELF (`sched_signal_fg`). Boot `init` is not the fg target (`sched_clear_fg`).

See [[features/Syscalls]], [[modules/Console]].
