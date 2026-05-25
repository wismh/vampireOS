#pragma once

#include <stdint.h>

void fb_init(void);
/* Packed user struct: width, height, pitch, phys. 0 ok, -1 if no FB. */
int fb_query(uint32_t *width, uint32_t *height, uint32_t *pitch, uint32_t *phys);
/* Fill [x, x+w) × [y, y+h) on the HHDM LFB. Clips; 0 ok, -1 if no FB. */
int fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
/* One bitmap-font row at the bottom of the LFB so `$` still types. */
void fb_overlay_putc(char c);
