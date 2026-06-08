/* Userland 8x8 glyphs. Plots via SYS_FBPIX into the shadow; caller presents. */
void font_puts(int x, int y, const char *s, unsigned scale, unsigned color);
