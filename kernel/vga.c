#include "vga.h"

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;

void vga_clear(void)
{
    int i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (uint16_t)' ' | (VGA_ATTR_WHITE << 8);
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
