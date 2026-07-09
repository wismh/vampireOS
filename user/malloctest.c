/* Allocate two blocks on brk, write, free one, malloc again, print ok. */
void *malloc(unsigned long n);
void free(void *p);
int printf(const char *fmt, ...);
void exit(int code);

int main(void)
{
    char *a;
    char *b;
    char *c;

    a = malloc(16);
    b = malloc(32);
    if (a == 0 || b == 0 || a == b) {
        exit(1);
    }
    a[0] = 'A';
    a[15] = 'a';
    b[0] = 'B';
    b[31] = 'b';
    if (a[0] != 'A' || a[15] != 'a' || b[0] != 'B' || b[31] != 'b') {
        exit(1);
    }
    free(a);
    c = malloc(16);
    if (c == 0) {
        exit(1);
    }
    c[0] = 'C';
    if (b[0] != 'B' || b[31] != 'b' || c[0] != 'C') {
        exit(1);
    }
    printf("ok");
    exit(0);
    return 0;
}
