#pragma once

enum { RTC_STR_LEN = 19 };

/* Latch CMOS once (ports 0x70/0x71). Later `date` adds PIT seconds. */
void rtc_init(void);
/* Wall clock as `YYYY-MM-DD HH:MM:SS` (19 bytes plus NUL). CMOS + PIT. */
void rtc_format(char *buf);
