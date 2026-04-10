/* Tiny libc string/mem. Linked into hi and sh; not open-coded there. */
void *memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d;
    const unsigned char *s;

    d = (unsigned char *)dst;
    s = (const unsigned char *)src;
    while (n != 0) {
        *d = *s;
        d++;
        s++;
        n--;
    }
    return dst;
}

unsigned long strlen(const char *s)
{
    unsigned long n;

    n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
