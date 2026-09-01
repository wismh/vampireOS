#pragma once

struct interrupt_frame;

void kbd_init(void);
void kbd_console_init(void);
void kbd_prompt(void);
/* `$` at the cursor while a task reads console stdin (init's `sh`). */
void kbd_stdin_prompt(void);
/* Redraw `$` / `kbd>` plus the current line onto the shadow (present separately). */
void kbd_overlay_refresh(void);
/* Blit VGA text + the prompt overlay onto the LFB. */
void kbd_console_sync(void);
void kbd_handle(struct interrupt_frame *frame);
/* Inject a cooked character (COM1 RX or the PS/2 map) into the line buffer. */
void kbd_feed(char c);
void kbd_set_irq_frame(struct interrupt_frame *frame);
/* 1 if a console stdin line is waiting for SYS_READ on fd 0. */
int kbd_stdin_ready(void);
/* Copy that line; >0 bytes, 0 empty, -2 none yet. */
int kbd_stdin_take(void *dst, unsigned max);
