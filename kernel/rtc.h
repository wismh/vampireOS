#pragma once

enum { RTC_STR_LEN = 19 };

/* CMOS RTC (0x70/0x71) as `YYYY-MM-DD HH:MM:SS` (19 bytes plus NUL). */
void rtc_format(char *buf);
