#include "rtc.h"
#include "idt.h"
#include "io.h"

#include <stdint.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71
#define CMOS_SEC 0x00
#define CMOS_MIN 0x02
#define CMOS_HOUR 0x04
#define CMOS_DAY 0x07
#define CMOS_MON 0x08
#define CMOS_YEAR 0x09
#define CMOS_STAT_A 0x0A
#define CMOS_STAT_B 0x0B
#define CMOS_CENT 0x32
#define CMOS_UIP 0x80u
#define CMOS_NMI_OFF 0x80u
#define CMOS_BIN 0x04u
#define CMOS_24H 0x02u
#define CMOS_HOUR_PM 0x80u
#define PIT_HZ 100u

static unsigned snap_y;
static unsigned snap_mon;
static unsigned snap_day;
static unsigned snap_h;
static unsigned snap_min;
static unsigned snap_sec;
static unsigned snap_ticks;
static int have_snap;

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, (uint8_t)(reg | CMOS_NMI_OFF));
    return inb(CMOS_DATA);
}

static unsigned from_bcd(unsigned v)
{
    return (v & 0x0Fu) + 10u * ((v >> 4) & 0x0Fu);
}

static void put2(char *p, unsigned v)
{
    p[0] = (char)('0' + (v / 10u) % 10u);
    p[1] = (char)('0' + v % 10u);
}

static unsigned month_days(unsigned y, unsigned m)
{
    if (m == 2u) {
        if ((y % 4u) == 0u && ((y % 100u) != 0u || (y % 400u) == 0u)) {
            return 29u;
        }
        return 28u;
    }
    if (m == 4u || m == 6u || m == 9u || m == 11u) {
        return 30u;
    }
    return 31u;
}

static void add_seconds(unsigned *y, unsigned *mon, unsigned *day,
                        unsigned *h, unsigned *min, unsigned *sec, unsigned add)
{
    unsigned t;
    unsigned dim;

    t = *sec + add;
    *sec = t % 60u;
    t = *min + t / 60u;
    *min = t % 60u;
    t = *h + t / 60u;
    *h = t % 24u;
    *day += t / 24u;
    for (;;) {
        dim = month_days(*y, *mon);
        if (*day <= dim) {
            break;
        }
        *day -= dim;
        (*mon)++;
        if (*mon > 12u) {
            *mon = 1u;
            (*y)++;
        }
    }
}

static void latch_cmos(void)
{
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t mon;
    uint8_t year;
    uint8_t cent;
    uint8_t stb;
    uint8_t sec2;
    unsigned y;
    unsigned c;
    unsigned h;
    int pm;
    int tries;

    tries = 0;
    do {
        while (cmos_read(CMOS_STAT_A) & CMOS_UIP) {
        }
        sec = cmos_read(CMOS_SEC);
        min = cmos_read(CMOS_MIN);
        hour = cmos_read(CMOS_HOUR);
        day = cmos_read(CMOS_DAY);
        mon = cmos_read(CMOS_MON);
        year = cmos_read(CMOS_YEAR);
        cent = cmos_read(CMOS_CENT);
        stb = cmos_read(CMOS_STAT_B);
        sec2 = cmos_read(CMOS_SEC);
        tries++;
    } while (sec2 != sec && tries < 4);

    pm = 0;
    if ((stb & CMOS_24H) == 0) {
        pm = (hour & CMOS_HOUR_PM) != 0;
        hour = (uint8_t)(hour & 0x7Fu);
    }
    if ((stb & CMOS_BIN) == 0) {
        sec = (uint8_t)from_bcd(sec);
        min = (uint8_t)from_bcd(min);
        hour = (uint8_t)from_bcd(hour);
        day = (uint8_t)from_bcd(day);
        mon = (uint8_t)from_bcd(mon);
        year = (uint8_t)from_bcd(year);
        cent = (uint8_t)from_bcd(cent);
    }
    h = hour;
    if ((stb & CMOS_24H) == 0) {
        if (h == 12u) {
            h = pm ? 12u : 0u;
        } else if (pm) {
            h += 12u;
        }
    }
    y = year;
    c = cent;
    if (c < 19u || c > 99u) {
        c = 20u;
    }
    y += c * 100u;
    snap_y = y;
    snap_mon = mon;
    snap_day = day;
    snap_h = h;
    snap_min = min;
    snap_sec = sec;
    snap_ticks = idt_ticks();
    have_snap = 1;
}

void rtc_init(void)
{
    if (have_snap != 0) {
        return;
    }
    latch_cmos();
}

void rtc_format(char *buf)
{
    unsigned y;
    unsigned mon;
    unsigned day;
    unsigned h;
    unsigned min;
    unsigned sec;
    unsigned add;

    rtc_init();
    y = snap_y;
    mon = snap_mon;
    day = snap_day;
    h = snap_h;
    min = snap_min;
    sec = snap_sec;
    add = (idt_ticks() - snap_ticks) / PIT_HZ;
    add_seconds(&y, &mon, &day, &h, &min, &sec, add);
    buf[0] = (char)('0' + (y / 1000u) % 10u);
    buf[1] = (char)('0' + (y / 100u) % 10u);
    buf[2] = (char)('0' + (y / 10u) % 10u);
    buf[3] = (char)('0' + y % 10u);
    buf[4] = '-';
    put2(buf + 5, mon);
    buf[7] = '-';
    put2(buf + 8, day);
    buf[10] = ' ';
    put2(buf + 11, h);
    buf[13] = ':';
    put2(buf + 14, min);
    buf[16] = ':';
    put2(buf + 17, sec);
    buf[19] = '\0';
}
