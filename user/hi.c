/* Freestanding C. printf from user/printf.c produces the line. */
int printf(const char *fmt, ...);
void exit(int code);

int main(void)
{
    printf("hi %d", 42);
    /* Exit so init can restart `$` after `hi > out`. `hi` stays at column 0. */
    exit(0);
    return 0;
}
