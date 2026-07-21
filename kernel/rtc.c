#include "rtc.h"
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

void rtc_format(char *buf)
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
