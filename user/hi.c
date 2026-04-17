/* Freestanding C. memcpy/strlen/strcmp from user/string.c produce the line. */
long write(int fd, const void *buf, unsigned long n);
void exit(int code);
void *memcpy(void *dst, const void *src, unsigned long n);
unsigned long strlen(const char *s);
int strcmp(const char *a, const char *b);

int main(void)
{
    static const char src[] = "hi";
    char buf[4];

    memcpy(buf, src, 3);
    if (strcmp(buf, src) != 0) {
        return 1;
    }
    write(1, buf, strlen(buf));
    /* Exit so init can restart `$` after `hi > out`. `hi` stays at column 0. */
    exit(0);
    return 0;
}
