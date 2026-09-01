#pragma once

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ATTR_WHITE 0x0F

void vga_clear(void);
void vga_set_cursor(int row, int col);
void vga_write_at(int row, int col, const char *msg);
void vga_write_dec_at(int row, int col, unsigned value);
void vga_write_hex64_at(int row, int col, uint64_t value);
void vga_putc(char c);
/* Glyph in the 80×25 cell (space if out of range). */
char vga_char_at(int row, int col);
