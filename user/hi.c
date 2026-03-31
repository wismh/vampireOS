/* Freestanding first C user program. Linked with crt.asm write/exit stubs. */
long write(int fd, const void *buf, unsigned long n);

int main(void)
{
    static const char msg[] = "hi";

    write(1, msg, 2);
    return 0;
}
