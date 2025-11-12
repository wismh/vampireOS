#include "vga.h"

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static int cursor_row = 3;
static int cursor_col = 0;

void vga_clear(void)
{
    int i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (uint16_t)' ' | (VGA_ATTR_WHITE << 8);
    }
    cursor_row = 3;
    cursor_col = 0;
}

static void vga_scroll(void)
{
    int i;

    for (i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    for (i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (uint16_t)' ' | (VGA_ATTR_WHITE << 8);
    }
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 4) & ~3;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga[cursor_row * VGA_WIDTH + cursor_col] =
                (uint16_t)' ' | (VGA_ATTR_WHITE << 8);
        }
    } else {
        vga[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)(uint8_t)c | (VGA_ATTR_WHITE << 8);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_write_at(int row, int col, const char *msg)
{
    int i;
    int pos = row * VGA_WIDTH + col;

    for (i = 0; msg[i] != '\0'; i++) {
        vga[pos + i] = (uint16_t)(uint8_t)msg[i] | (VGA_ATTR_WHITE << 8);
    }
}

void vga_write_dec_at(int row, int col, unsigned value)
{
    char buf[11];
    int n = 0;
    unsigned v = value;

    if (v == 0) {
        buf[n++] = '0';
    } else {
        while (v > 0 && n < 10) {
            buf[n++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }

    {
        int pos = row * VGA_WIDTH + col;

        while (n > 0) {
            n--;
            vga[pos++] = (uint16_t)(uint8_t)buf[n] | (VGA_ATTR_WHITE << 8);
        }
    }
}
