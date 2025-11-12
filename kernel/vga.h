#pragma once

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ATTR_WHITE 0x0F

void vga_clear(void);
void vga_write_at(int row, int col, const char *msg);
void vga_write_dec_at(int row, int col, unsigned value);
void vga_putc(char c);
