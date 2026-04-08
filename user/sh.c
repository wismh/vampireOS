/* User shell: exec argv[1], or a line from stdin (fd 0). Boot and `run sh`. */
long write(int fd, const void *buf, unsigned long n);
long read(int fd, void *buf, unsigned long n);
long exec(const char *path);

int main(int argc, char **argv)
{
    char buf[40];
    const char *name;
    long n;
    long i;

    name = 0;
    if (argc >= 2 && argv != 0 && argv[1] != 0 && argv[1][0] != '\0') {
        name = argv[1];
    } else {
        write(1, "$", 1);
        n = read(0, buf, 39);
        if (n > 0) {
            if (n > 39) {
                n = 39;
            }
            buf[n] = '\0';
            while (n > 0) {
                char c = buf[n - 1];

                if (c != '\n' && c != '\r' && c != ' ') {
                    break;
                }
                n--;
                buf[n] = '\0';
            }
            i = 0;
            while (buf[i] == ' ') {
                i++;
            }
            if (buf[i] != '\0') {
                name = buf + i;
            }
        }
    }
    if (name == 0) {
        write(1, "?", 1);
        return 1;
    }
    exec(name);
    write(1, "?", 1);
    return 1;
}
