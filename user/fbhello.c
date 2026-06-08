/* Draw `hello` on the LFB with a userland font. SYS_FBPIX into the shadow, then SYS_FBPRESENT. */
#include "font.h"

long write(int fd, const void *buf, unsigned long n);
void exit(int code);
long fbinfo(unsigned *info);
long fbpresent(void);

enum { FONT_SCALE = 8 };

int main(void)
{
    unsigned info[4];
    unsigned w;
    unsigned h;
    unsigned glyph;
    int x;
    int y;

    if (fbinfo(info) != 0) {
        write(1, "X", 1);
        exit(1);
    }
    w = info[0];
    h = info[1];
    glyph = 8u * (unsigned)FONT_SCALE;
    if (w < 5u * glyph || h < glyph) {
        write(1, "X", 1);
        exit(1);
    }
    x = (int)((w - 5u * glyph) / 2u);
    y = (int)(h / 2u);
    font_puts(x, y, "hello", (unsigned)FONT_SCALE, 0x00F4E4C8u);
    if (fbpresent() != 0) {
        write(1, "X", 1);
        exit(1);
    }
    write(1, "ok", 2);
    exit(0);
    return 0;
}
