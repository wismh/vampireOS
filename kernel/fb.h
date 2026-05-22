#pragma once

#include <stdint.h>

void fb_init(void);
/* Packed user struct: width, height, pitch, phys. 0 ok, -1 if no FB. */
int fb_query(uint32_t *width, uint32_t *height, uint32_t *pitch, uint32_t *phys);
