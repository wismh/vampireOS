/* Second PT_LOAD: initialized .data plus an uninitialized .bss tail. */
long write(int fd, const void *buf, unsigned long n);
void exit(int code);

static int mark = 0x5a;
static unsigned char pad[128];

int main(void)
{
    unsigned i;

    if (mark != 0x5a) {
        write(1, "bss bad", 7);
        exit(1);
    }
    for (i = 0; i < 128u; i++) {
        if (pad[i] != 0) {
            write(1, "bss bad", 7);
            exit(1);
        }
        pad[i] = (unsigned char)(i + 1u);
    }
    if (pad[0] != 1 || pad[127] != 128) {
        write(1, "bss bad", 7);
        exit(1);
    }
    write(1, "bss ok", 6);
    exit(0);
    return 0;
}
