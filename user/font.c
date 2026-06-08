/* Tiny 8x8 ASCII subset (enough for `hello`). Each lit pixel is a SYS_FBPIX rect on the shadow. */
#include "font.h"

long fbpix(const unsigned *rect);

enum { FONT_W = 8 };

/* MSB is the leftmost pixel. Only the letters in `hello`. */
static const unsigned char g_e[FONT_W] = {
    0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00
};
static const unsigned char g_h[FONT_W] = {
    0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00
};
static const unsigned char g_l[FONT_W] = {
    0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00
};
static const unsigned char g_o[FONT_W] = {
    0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00
};

static const unsigned char *glyph_for(char c)
{
    if (c == 'e') {
        return g_e;
    }
    if (c == 'h') {
        return g_h;
    }
    if (c == 'l') {
        return g_l;
    }
    if (c == 'o') {
        return g_o;
    }
    return 0;
}

static void font_putc(int x, int y, char c, unsigned scale, unsigned color)
{
    const unsigned char *row;
    unsigned gy;
    unsigned gx;
    unsigned rect[5];

    row = glyph_for(c);
    if (row == 0 || scale == 0) {
        return;
    }
    rect[2] = scale;
    rect[3] = scale;
    rect[4] = color;
    for (gy = 0; gy < 8; gy++) {
        unsigned char bits = row[gy];

        for (gx = 0; gx < 8; gx++) {
            if ((bits & (unsigned char)(0x80u >> gx)) == 0) {
                continue;
            }
            rect[0] = (unsigned)x + gx * scale;
            rect[1] = (unsigned)y + gy * scale;
            (void)fbpix(rect);
        }
    }
}

void font_puts(int x, int y, const char *s, unsigned scale, unsigned color)
{
    int cx;

    if (s == 0 || scale == 0) {
        return;
    }
    cx = x;
    while (*s != '\0') {
        font_putc(cx, y, *s, scale, color);
        cx += (int)(8u * scale);
        s++;
    }
}
