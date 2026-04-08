#pragma once

void kbd_init(void);
void kbd_console_init(void);
void kbd_prompt(void);
/* `$` at the cursor while a task reads console stdin (boot `sh`). */
void kbd_stdin_prompt(void);
void kbd_handle(void);
/* 1 if a console stdin line is waiting for SYS_READ on fd 0. */
int kbd_stdin_ready(void);
/* Copy that line; >0 bytes, 0 empty, -2 none yet. */
int kbd_stdin_take(void *dst, unsigned max);
