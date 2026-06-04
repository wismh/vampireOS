#include "fb.h"
#include "vmm.h"

#include <stdint.h>

/* Must match boot/const.inc. */
#define FB_BOOT_PHYS 0x4F00ull
#define FB_BOOT_MAGIC 0x31424656u
#define FB_SCALE 4
#define FB_BG 0x001A0A12u
#define FB_FG 0x00F4E4C8u

struct fb_boot {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t phys;
} __attribute__((packed));

static volatile uint8_t *fb_mem;
static uint32_t fb_w;
static uint32_t fb_h;
static uint32_t fb_pitch;
static uint32_t fb_bpp;
static uint32_t fb_phys;
static int overlay_col;

/* 8x8, ASCII 32..126, MSB is the leftmost pixel. */
static const uint8_t font8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, /* ! */
    {0x6C, 0x6C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, /* " */
    {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00}, /* # */
    {0x18, 0x3E, 0x58, 0x3C, 0x1A, 0x7C, 0x18, 0x00}, /* $ */
    {0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00}, /* % */
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00}, /* & */
    {0x18, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, /* ( */
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, /* ) */
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, /* * */
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, /* , */
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, /* . */
    {0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, /* / */
    {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00}, /* 0 */
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, /* 1 */
    {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00}, /* 2 */
    {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, /* 3 */
    {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00}, /* 4 */
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, /* 5 */
    {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00}, /* 6 */
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, /* 7 */
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, /* 8 */
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00}, /* 9 */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, /* : */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30}, /* ; */
    {0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00}, /* < */
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, /* = */
    {0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00}, /* > */
    {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00}, /* ? */
    {0x3C, 0x66, 0x6E, 0x6A, 0x6E, 0x60, 0x3C, 0x00}, /* @ */
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, /* A */
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, /* B */
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, /* C */
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, /* D */
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, /* E */
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, /* F */
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00}, /* G */
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, /* H */
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, /* I */
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00}, /* J */
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, /* K */
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, /* L */
    {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00}, /* M */
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, /* N */
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* O */
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, /* P */
    {0x3C, 0x66, 0x66, 0x66, 0x76, 0x6C, 0x36, 0x00}, /* Q */
    {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00}, /* R */
    {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00}, /* S */
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, /* T */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* U */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, /* V */
    {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00}, /* W */
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, /* X */
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, /* Y */
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, /* Z */
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, /* [ */
    {0x40, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00}, /* \ */
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, /* ] */
    {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, /* _ */
    {0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ` */
    {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00}, /* a */
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00}, /* b */
    {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00}, /* c */
    {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00}, /* d */
    {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00}, /* e */
    {0x1C, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x00}, /* f */
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C}, /* g */
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, /* h */
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, /* i */
    {0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38}, /* j */
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, /* k */
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, /* l */
    {0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xC6, 0xC6, 0x00}, /* m */
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, /* n */
    {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* o */
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60}, /* p */
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06}, /* q */
    {0x00, 0x00, 0x6C, 0x76, 0x60, 0x60, 0x60, 0x00}, /* r */
    {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00}, /* s */
    {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00}, /* t */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, /* u */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, /* v */
    {0x00, 0x00, 0xC6, 0xD6, 0xFE, 0x7C, 0x6C, 0x00}, /* w */
    {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00}, /* x */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C}, /* y */
    {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00}, /* z */
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, /* { */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, /* | */
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, /* } */
    {0x32, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ~ */
};

static void fb_pixel(int x, int y, uint32_t color)
{
    volatile uint8_t *p;
    unsigned bytes;

    if (x < 0 || y < 0 || (uint32_t)x >= fb_w || (uint32_t)y >= fb_h) {
        return;
    }
    bytes = fb_bpp / 8u;
    p = fb_mem + (uint32_t)y * fb_pitch + (uint32_t)x * bytes;
    p[0] = (uint8_t)color;
    p[1] = (uint8_t)(color >> 8);
    p[2] = (uint8_t)(color >> 16);
    if (fb_bpp == 32) {
        p[3] = (uint8_t)(color >> 24);
    }
}

static void fb_fill(uint32_t color)
{
    uint32_t y;
    uint32_t x;

    if (fb_bpp == 32 && (fb_pitch & 3u) == 0) {
        for (y = 0; y < fb_h; y++) {
            volatile uint32_t *row =
                (volatile uint32_t *)(fb_mem + (uint64_t)y * fb_pitch);

            for (x = 0; x < fb_w; x++) {
                row[x] = color;
            }
        }
        return;
    }

    for (y = 0; y < fb_h; y++) {
        for (x = 0; x < fb_w; x++) {
            fb_pixel((int)x, (int)y, color);
        }
    }
}

static void fb_glyph(int x, int y, char c, uint32_t color)
{
    const uint8_t *row;
    int gy;
    int gx;
    int sy;
    int sx;
    unsigned idx;

    if (c < 32 || c > 126) {
        c = '?';
    }
    idx = (unsigned)(c - 32);
    row = font8x8[idx];
    for (gy = 0; gy < 8; gy++) {
        uint8_t bits = row[gy];

        for (gx = 0; gx < 8; gx++) {
            if ((bits & (uint8_t)(0x80u >> gx)) == 0) {
                continue;
            }
            for (sy = 0; sy < FB_SCALE; sy++) {
                for (sx = 0; sx < FB_SCALE; sx++) {
                    fb_pixel(x + gx * FB_SCALE + sx, y + gy * FB_SCALE + sy, color);
                }
            }
        }
    }
}

static void fb_banner(const char *msg)
{
    int n = 0;
    int i;
    int x;
    int y;
    int width;

    while (msg[n] != '\0') {
        n++;
    }
    width = n * 8 * FB_SCALE;
    x = 8 * FB_SCALE;
    if (fb_w > (uint32_t)width + 16u * FB_SCALE) {
        x = (int)((fb_w - (uint32_t)width) / 2u);
    }
    y = (int)(fb_h / 4u);
    for (i = 0; i < n; i++) {
        fb_glyph(x + i * 8 * FB_SCALE, y, msg[i], FB_FG);
    }
}

void fb_init(void)
{
    const struct fb_boot *info;
    uint64_t phys;
    uint64_t bytes;
    uint32_t min_pitch;

    fb_mem = 0;
    fb_w = 0;
    fb_h = 0;
    fb_pitch = 0;
    fb_bpp = 0;
    fb_phys = 0;
    info = (const struct fb_boot *)(uintptr_t)phys_to_virt(FB_BOOT_PHYS);
    if (info == 0 || info->magic != FB_BOOT_MAGIC) {
        return;
    }
    if (info->width < 320 || info->height < 200 || info->phys == 0) {
        return;
    }
    if (info->bpp != 32 && info->bpp != 24) {
        return;
    }
    min_pitch = info->width * (info->bpp / 8u);
    if (info->pitch < min_pitch) {
        return;
    }

    bytes = (uint64_t)info->pitch * (uint64_t)info->height;
    phys = (uint64_t)info->phys;
    if (vmm_map_mmio(phys, bytes) != 0) {
        return;
    }

    fb_w = info->width;
    fb_h = info->height;
    fb_pitch = info->pitch;
    fb_bpp = info->bpp;
    fb_phys = info->phys;
    fb_mem = (volatile uint8_t *)(uintptr_t)phys_to_virt(phys);
    overlay_col = 0;
    fb_fill(FB_BG);
    fb_banner("Vampire OS");
}

int fb_query(uint32_t *width, uint32_t *height, uint32_t *pitch, uint32_t *phys)
{
    if (fb_mem == 0 || fb_w == 0 || fb_h == 0 || fb_phys == 0) {
        return -1;
    }
    if (width == 0 || height == 0 || pitch == 0 || phys == 0) {
        return -1;
    }
    *width = fb_w;
    *height = fb_h;
    *pitch = fb_pitch;
    *phys = fb_phys;
    return 0;
}

int fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    uint32_t x1;
    uint32_t y1;
    uint32_t xx;
    uint32_t yy;

    if (fb_mem == 0 || w == 0 || h == 0) {
        return -1;
    }
    if (x >= fb_w || y >= fb_h) {
        return -1;
    }
    x1 = x + w;
    y1 = y + h;
    if (x1 < x || x1 > fb_w) {
        x1 = fb_w;
    }
    if (y1 < y || y1 > fb_h) {
        y1 = fb_h;
    }
    if (fb_bpp == 32 && (fb_pitch & 3u) == 0) {
        for (yy = y; yy < y1; yy++) {
            volatile uint32_t *row =
                (volatile uint32_t *)(fb_mem + (uint64_t)yy * fb_pitch);

            for (xx = x; xx < x1; xx++) {
                row[xx] = color;
            }
        }
        return 0;
    }
    for (yy = y; yy < y1; yy++) {
        for (xx = x; xx < x1; xx++) {
            fb_pixel((int)xx, (int)yy, color);
        }
    }
    return 0;
}

#define FB_OVERLAY_PAD 8
#define FB_PROMPT_SCALE 2

static int fb_prompt_cell(void)
{
    return 8 * FB_PROMPT_SCALE;
}

static int fb_overlay_y(void)
{
    int cell;
    int y;

    cell = fb_prompt_cell();
    if (fb_mem == 0 || fb_h < (uint32_t)cell + FB_OVERLAY_PAD) {
        return -1;
    }
    y = (int)fb_h - cell - FB_OVERLAY_PAD;
    return y;
}

static void fb_overlay_cell(int x, int y, char c)
{
    const uint8_t *row;
    int gy;
    int gx;
    int sy;
    int sx;
    unsigned idx;

    if (c < 32 || c > 126) {
        c = ' ';
    }
    idx = (unsigned)(c - 32);
    row = font8x8[idx];
    for (gy = 0; gy < 8; gy++) {
        uint8_t bits = row[gy];

        for (gx = 0; gx < 8; gx++) {
            uint32_t color = FB_BG;

            if ((bits & (uint8_t)(0x80u >> gx)) != 0) {
                color = FB_FG;
            }
            for (sy = 0; sy < FB_PROMPT_SCALE; sy++) {
                for (sx = 0; sx < FB_PROMPT_SCALE; sx++) {
                    fb_pixel(x + gx * FB_PROMPT_SCALE + sx,
                             y + gy * FB_PROMPT_SCALE + sy, color);
                }
            }
        }
    }
}

static void fb_overlay_clear_from(int col)
{
    int y = fb_overlay_y();
    int x;
    int cell = fb_prompt_cell();

    if (y < 0 || col < 0) {
        return;
    }
    for (;;) {
        x = col * cell;
        if ((uint32_t)x + (uint32_t)cell > fb_w) {
            break;
        }
        fb_overlay_cell(x, y, ' ');
        col++;
    }
}

void fb_overlay_putc(char c)
{
    int y;
    int x;
    int cell = fb_prompt_cell();

    y = fb_overlay_y();
    if (y < 0) {
        return;
    }
    if (c == '\n') {
        overlay_col = 0;
        fb_overlay_clear_from(0);
        return;
    }
    if (c == '\b') {
        if (overlay_col > 0) {
            overlay_col--;
            x = overlay_col * cell;
            if ((uint32_t)x + (uint32_t)cell <= fb_w) {
                fb_overlay_cell(x, y, ' ');
            }
        }
        return;
    }
    if (c == '\t') {
        overlay_col = (overlay_col + 4) & ~3;
        return;
    }
    x = overlay_col * cell;
    if ((uint32_t)x + (uint32_t)cell > fb_w) {
        return;
    }
    fb_overlay_cell(x, y, c);
    overlay_col++;
}

void fb_prompt_line(const char *prompt, const char *buf, unsigned len)
{
    int y;
    int x;
    int col;
    unsigned i;
    int cell = fb_prompt_cell();

    y = fb_overlay_y();
    if (y < 0) {
        return;
    }
    fb_overlay_clear_from(0);
    col = 0;
    if (prompt != 0) {
        while (prompt[col] != '\0') {
            x = col * cell;
            if ((uint32_t)x + (uint32_t)cell > fb_w) {
                overlay_col = col;
                return;
            }
            fb_overlay_cell(x, y, prompt[col]);
            col++;
        }
    }
    if (buf == 0) {
        overlay_col = col;
        return;
    }
    for (i = 0; i < len; i++) {
        x = col * cell;
        if ((uint32_t)x + (uint32_t)cell > fb_w) {
            break;
        }
        fb_overlay_cell(x, y, buf[i]);
        col++;
    }
    overlay_col = col;
}

static uint32_t fb_get(int x, int y)
{
    volatile uint8_t *p;
    unsigned bytes;
    uint32_t color;

    if (fb_mem == 0 || x < 0 || y < 0 || (uint32_t)x >= fb_w || (uint32_t)y >= fb_h) {
        return 0;
    }
    bytes = fb_bpp / 8u;
    p = fb_mem + (uint32_t)y * fb_pitch + (uint32_t)x * bytes;
    color = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    if (fb_bpp == 32) {
        color |= (uint32_t)p[3] << 24;
    }
    return color;
}

static void fb_xor_pixel(int x, int y)
{
    fb_pixel(x, y, fb_get(x, y) ^ 0x00FFFFFFu);
}

void fb_pointer(int x, int y)
{
    int d;

    if (fb_mem == 0) {
        return;
    }
    fb_xor_pixel(x, y);
    for (d = 1; d <= 2; d++) {
        fb_xor_pixel(x + d, y);
        fb_xor_pixel(x - d, y);
        fb_xor_pixel(x, y + d);
        fb_xor_pixel(x, y - d);
    }
}

void fb_draw_text(int x, int y, const char *s)
{
    int col;
    int cell = fb_prompt_cell();

    if (fb_mem == 0 || s == 0 || y < 0) {
        return;
    }
    col = 0;
    while (s[col] != '\0') {
        int px = x + col * cell;

        if (px < 0 || (uint32_t)px + (uint32_t)cell > fb_w) {
            break;
        }
        if ((uint32_t)y + (uint32_t)cell > fb_h) {
            break;
        }
        fb_overlay_cell(px, y, s[col]);
        col++;
    }
}
