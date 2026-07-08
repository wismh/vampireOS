/* Freestanding printf. %s %d %x only; printf writes fd 1. */
long write(int fd, const void *buf, unsigned long n);

enum { PRINTF_MAX = 256 };

static void putc_at(char *dst, unsigned long cap, unsigned long *pos, char c)
{
    if (dst != 0 && cap != 0 && *pos + 1ul < cap) {
        dst[*pos] = c;
    }
    (*pos)++;
}

static void put_uint(char *dst, unsigned long cap, unsigned long *pos,
                     unsigned v, unsigned base)
{
    char tmp[16];
    int i;

    if (v == 0u) {
        putc_at(dst, cap, pos, '0');
        return;
    }
    i = 0;
    while (v != 0u && i < 16) {
        unsigned d;

        d = v % base;
        tmp[i] = (char)(d < 10u ? '0' + d : 'a' + (d - 10u));
        i++;
        v /= base;
    }
    while (i > 0) {
        i--;
        putc_at(dst, cap, pos, tmp[i]);
    }
}

static void put_str(char *dst, unsigned long cap, unsigned long *pos,
                    const char *s)
{
    if (s == 0) {
        s = "";
    }
    while (*s != '\0') {
        putc_at(dst, cap, pos, *s);
        s++;
    }
}

static int vsnprintf(char *dst, unsigned long cap, const char *fmt,
                     __builtin_va_list ap)
{
    unsigned long pos;
    const char *s;
    int v;
    unsigned u;

    pos = 0;
    if (fmt == 0) {
        fmt = "";
    }
    while (*fmt != '\0') {
        if (*fmt != '%') {
            putc_at(dst, cap, &pos, *fmt);
            fmt++;
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            putc_at(dst, cap, &pos, '%');
            fmt++;
            continue;
        }
        if (*fmt == 's') {
            s = __builtin_va_arg(ap, const char *);
            put_str(dst, cap, &pos, s);
            fmt++;
            continue;
        }
        if (*fmt == 'd') {
            v = __builtin_va_arg(ap, int);
            if (v < 0) {
                putc_at(dst, cap, &pos, '-');
                u = (unsigned)(-(v + 1)) + 1u;
            } else {
                u = (unsigned)v;
            }
            put_uint(dst, cap, &pos, u, 10u);
            fmt++;
            continue;
        }
        if (*fmt == 'x') {
            u = __builtin_va_arg(ap, unsigned);
            put_uint(dst, cap, &pos, u, 16u);
            fmt++;
            continue;
        }
        putc_at(dst, cap, &pos, '%');
        if (*fmt != '\0') {
            putc_at(dst, cap, &pos, *fmt);
            fmt++;
        }
    }
    if (dst != 0 && cap != 0) {
        unsigned long t;

        t = pos;
        if (t >= cap) {
            t = cap - 1ul;
        }
        dst[t] = '\0';
    }
    return (int)pos;
}

int snprintf(char *dst, unsigned long n, const char *fmt, ...)
{
    __builtin_va_list ap;
    int r;

    __builtin_va_start(ap, fmt);
    r = vsnprintf(dst, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}

int printf(const char *fmt, ...)
{
    char buf[PRINTF_MAX];
    __builtin_va_list ap;
    int n;

    __builtin_va_start(ap, fmt);
    n = vsnprintf(buf, (unsigned long)PRINTF_MAX, fmt, ap);
    __builtin_va_end(ap);
    if (n < 0) {
        return n;
    }
    if (n >= PRINTF_MAX) {
        n = PRINTF_MAX - 1;
    }
    if (n > 0) {
        (void)write(1, buf, (unsigned long)n);
    }
    return n;
}
