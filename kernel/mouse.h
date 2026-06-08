#pragma once

#include <stdint.h>

void mouse_init(void);
void mouse_on_byte(uint8_t data);
/* XOR the pointer back on after a present wipes the scanout. */
void mouse_repaint(void);
