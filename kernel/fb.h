#pragma once

#include <stdint.h>

void fb_init(void);
/* Packed user struct: width, height, pitch, phys. 0 ok, -1 if no FB. */
int fb_query(uint32_t *width, uint32_t *height, uint32_t *pitch, uint32_t *phys);
/* Fill [x, x+w) × [y, y+h) on the shadow (or LFB if no back). Clips; 0 ok, -1 if no FB. */
int fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
/* Copy the PMM shadow onto the LFB in one blit. 0 ok, -1 if no FB. */
int fb_present(void);
/* 1 if [y, y+h) overlaps the prompt overlay row (so `$` must be redrawn). */
int fb_hits_overlay(uint32_t y, uint32_t h);
/* One bitmap-font row at the bottom of the LFB so `$` still types. */
void fb_overlay_putc(char c);
/* Redraw `$` / `kbd>` plus the current line buffer on that bottom row. */
void fb_prompt_line(const char *prompt, const char *buf, unsigned len);
/* Paint the 80×25 VGA text buffer onto the shadow (8×8, top-left). */
void fb_vga_blit(void);
/* XOR a small cross at (x, y) so the mouse pointer can move. */
void fb_pointer(int x, int y);
/* Opaque bitmap-font string on the LFB (click `x,y`, not the prompt row). */
void fb_draw_text(int x, int y, const char *s);
