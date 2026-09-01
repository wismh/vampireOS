#include "vga.h"
#include "serial.h"
#include "vmm.h"

#define VGA_PHYS 0xB8000ull

static int cursor_row = 3;
static int cursor_col = 0;
/* VBE takes the scanout; 0xB8000 is not readable as text. Keep a copy. */
static uint16_t cells[VGA_WIDTH * VGA_HEIGHT];

static volatile uint16_t *vga_buf(void)
{
    return (volatile uint16_t *)(uintptr_t)phys_to_virt(VGA_PHYS);
}

static void vga_poke(int pos, char c)
{
    uint16_t v = (uint16_t)(uint8_t)c | (VGA_ATTR_WHITE << 8);

    cells[pos] = v;
    vga_buf()[pos] = v;
}

void vga_clear(void)
{
    int i;

    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_poke(i, ' ');
    }
    cursor_row = 3;
    cursor_col = 0;
}

void vga_set_cursor(int row, int col)
{
    cursor_row = row;
    cursor_col = col;
}

static void vga_scroll(void)
{
    int i;

    for (i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        char c = (char)(uint8_t)cells[i + VGA_WIDTH];

        vga_poke(i, c);
    }
    for (i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_poke(i, ' ');
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
            vga_poke(cursor_row * VGA_WIDTH + cursor_col, ' ');
        }
    } else {
        vga_poke(cursor_row * VGA_WIDTH + cursor_col, c);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
    serial_putc(c);
}

char vga_char_at(int row, int col)
{
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return ' ';
    }
    return (char)(uint8_t)cells[row * VGA_WIDTH + col];
}

void vga_write_at(int row, int col, const char *msg)
{
    int i;
    int pos;

    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH || msg == 0) {
        return;
    }
    pos = row * VGA_WIDTH + col;
    for (i = 0; msg[i] != '\0' && col + i < VGA_WIDTH; i++) {
        vga_poke(pos + i, msg[i]);
    }
}

void vga_write_hex64_at(int row, int col, uint64_t value)
{
    int i;
    int pos;

    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }
    pos = row * VGA_WIDTH + col;
    for (i = 15; i >= 0; i--) {
        unsigned nibble = (unsigned)((value >> (i * 4)) & 0xF);
        char c = (char)(nibble < 10 ? '0' + nibble : 'a' + (nibble - 10));

        if (col + (15 - i) >= VGA_WIDTH) {
            break;
        }
        vga_poke(pos + (15 - i), c);
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

    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }
    {
        int pos = row * VGA_WIDTH + col;

        while (n > 0 && col < VGA_WIDTH) {
            n--;
            vga_poke(pos++, buf[n]);
            col++;
        }
    }
}
