#include "mouse.h"
#include "fb.h"
#include "io.h"
#include "vga.h"

#include <stdint.h>

#define I8042_DATA 0x60
#define I8042_STATUS 0x64
#define I8042_STATUS_OUT 0x01
#define I8042_STATUS_IN 0x02
#define I8042_STATUS_AUX 0x20
#define I8042_TIMEOUT 100000u
#define MOUSE_PKT 3
#define MOUSE_ALWAYS1 0x08
#define MOUSE_OVERFLOW 0xC0
#define MOUSE_LEFT 0x01

static int mx;
static int my;
static int max_x;
static int max_y;
static int ptr_x;
static int ptr_y;
static int ptr_on;
static int left_down;
static uint8_t pkt[MOUSE_PKT];
static int pkt_i;

static int wait_in_empty(void)
{
    unsigned i;

    for (i = 0; i < I8042_TIMEOUT; i++) {
        if ((inb(I8042_STATUS) & I8042_STATUS_IN) == 0) {
            return 0;
        }
    }
    return -1;
}

static int wait_out_full(void)
{
    unsigned i;

    for (i = 0; i < I8042_TIMEOUT; i++) {
        if ((inb(I8042_STATUS) & I8042_STATUS_OUT) != 0) {
            return 0;
        }
    }
    return -1;
}

static int mouse_read(uint8_t *out)
{
    unsigned i;

    for (i = 0; i < I8042_TIMEOUT; i++) {
        uint8_t st = inb(I8042_STATUS);

        if ((st & I8042_STATUS_OUT) == 0) {
            continue;
        }
        {
            uint8_t data = inb(I8042_DATA);

            if ((st & I8042_STATUS_AUX) != 0) {
                *out = data;
                return 0;
            }
        }
    }
    return -1;
}

static int mouse_write(uint8_t cmd)
{
    if (wait_in_empty() != 0) {
        return -1;
    }
    outb(I8042_STATUS, 0xD4);
    if (wait_in_empty() != 0) {
        return -1;
    }
    outb(I8042_DATA, cmd);
    return 0;
}

static void put_num(char *buf, unsigned *n, unsigned v)
{
    char tmp[10];
    int k = 0;

    if (v == 0) {
        buf[(*n)++] = '0';
        return;
    }
    while (v > 0 && k < 10) {
        tmp[k++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (k > 0) {
        k--;
        buf[(*n)++] = tmp[k];
    }
}

static void clamp_pos(void)
{
    if (mx < 0) {
        mx = 0;
    }
    if (my < 0) {
        my = 0;
    }
    if (max_x > 0 && mx >= max_x) {
        mx = max_x - 1;
    }
    if (max_y > 0 && my >= max_y) {
        my = max_y - 1;
    }
}

static void move_pointer(void)
{
    if (ptr_on != 0) {
        fb_pointer(ptr_x, ptr_y);
    }
    ptr_x = mx;
    ptr_y = my;
    fb_pointer(ptr_x, ptr_y);
    ptr_on = 1;
}

static void print_click(void)
{
    char buf[16];
    unsigned n = 0;

    put_num(buf, &n, (unsigned)mx);
    buf[n++] = ',';
    put_num(buf, &n, (unsigned)my);
    while (n < 12u) {
        buf[n++] = ' ';
    }
    buf[n] = '\0';
    vga_write_at(2, 0, buf);
    fb_draw_text(8, 8, buf);
}

void mouse_init(void)
{
    uint8_t cfg;
    uint8_t ack;
    uint32_t w;
    uint32_t h;
    uint32_t pitch;
    uint32_t phys;

    mx = 320;
    my = 240;
    max_x = 640;
    max_y = 480;
    ptr_on = 0;
    left_down = 0;
    pkt_i = 0;
    if (fb_query(&w, &h, &pitch, &phys) == 0) {
        max_x = (int)w;
        max_y = (int)h;
        mx = (int)(w / 2u);
        my = (int)(h / 2u);
    }

    if (wait_in_empty() != 0) {
        return;
    }
    outb(I8042_STATUS, 0xA8);

    if (wait_in_empty() != 0) {
        return;
    }
    outb(I8042_STATUS, 0x20);
    if (wait_out_full() != 0) {
        return;
    }
    cfg = inb(I8042_DATA);
    cfg |= 0x02;
    cfg &= (uint8_t)~0x20u;
    if (wait_in_empty() != 0) {
        return;
    }
    outb(I8042_STATUS, 0x60);
    if (wait_in_empty() != 0) {
        return;
    }
    outb(I8042_DATA, cfg);

    if (mouse_write(0xF4) != 0) {
        return;
    }
    if (mouse_read(&ack) != 0) {
        return;
    }
    (void)ack;
    move_pointer();
}

void mouse_on_byte(uint8_t data)
{
    int dx;
    int dy;
    int left;

    if (pkt_i == 0 && (data & MOUSE_ALWAYS1) == 0) {
        return;
    }
    pkt[pkt_i++] = data;
    if (pkt_i < MOUSE_PKT) {
        return;
    }
    pkt_i = 0;
    if ((pkt[0] & MOUSE_OVERFLOW) != 0) {
        return;
    }

    dx = (int)(int8_t)pkt[1];
    dy = (int)(int8_t)pkt[2];
    mx += dx;
    my -= dy;
    clamp_pos();
    move_pointer();

    left = (pkt[0] & MOUSE_LEFT) != 0;
    if (left != 0 && left_down == 0) {
        print_click();
    }
    left_down = left;
}
