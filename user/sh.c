/* User shell: one `<` or `>`; fork/exec so `cat out` keeps `hi` on VGA. */
long write(int fd, const void *buf, unsigned long n);
long read(int fd, void *buf, unsigned long n);
long exec(const char *path);
long open(const char *path, long flags);
long dup2(int oldfd, int newfd);
long close(int fd);
long fork(void);
long wait(long pid);
void exit(int code);
int strcmp(const char *a, const char *b);

static void fail(void)
{
    write(1, "?", 1);
    exit(1);
}

static char *skip_ws(char *s)
{
    while (*s == ' ') {
        s++;
    }
    return s;
}

static void trim_end(char *s)
{
    long n;

    n = 0;
    while (s[n] != '\0') {
        n++;
    }
    while (n > 0) {
        char c = s[n - 1];

        if (c != ' ' && c != '\n' && c != '\r') {
            break;
        }
        n--;
        s[n] = '\0';
    }
}

static char *cut_word(char *s)
{
    while (*s != '\0' && *s != ' ') {
        s++;
    }
    if (*s != '\0') {
        *s = '\0';
        s++;
    }
    return s;
}

static int apply_redir(const char *path, int dstfd, long flags)
{
    long fd;

    fd = open(path, flags);
    if (fd < 0) {
        return -1;
    }
    if (dup2((int)fd, dstfd) < 0) {
        return -1;
    }
    if ((int)fd != dstfd) {
        (void)close((int)fd);
    }
    return 0;
}

static int parse_line(char **namep, char **in_path, char **out_path)
{
    char *name;
    char *p;
    char redir;

    name = *namep;
    *in_path = 0;
    *out_path = 0;
    redir = 0;
    p = name;
    while (*p != '\0') {
        if (*p == '<' || *p == '>') {
            if (redir != 0) {
                return -1;
            }
            redir = *p;
            *p = '\0';
            p = skip_ws(p + 1);
            if (redir == '<') {
                *in_path = p;
            } else {
                *out_path = p;
            }
            break;
        }
        p++;
    }
    if (*in_path != 0) {
        (void)cut_word(*in_path);
        trim_end(*in_path);
        if ((*in_path)[0] == '\0') {
            return -1;
        }
    }
    if (*out_path != 0) {
        (void)cut_word(*out_path);
        trim_end(*out_path);
        if ((*out_path)[0] == '\0') {
            return -1;
        }
    }
    trim_end(name);
    name = skip_ws(name);
    if (name[0] == '\0') {
        return -1;
    }
    /* First word is the ELF; a leftover word is stdin (`cat out`). */
    p = cut_word(name);
    p = skip_ws(p);
    if (*p != '\0' && *in_path == 0) {
        (void)cut_word(p);
        trim_end(p);
        if (p[0] != '\0') {
            *in_path = p;
        }
    }
    *namep = name;
    return 0;
}

int main(int argc, char **argv)
{
    char buf[40];
    char *name;
    char *p;
    char *in_path;
    char *out_path;
    long n;
    long pid;
    int once;

    once = 0;
    for (;;) {
        name = 0;
        in_path = 0;
        out_path = 0;
        if (once == 0 && argc >= 2 && argv != 0 && argv[1] != 0 &&
            strcmp(argv[1], "") != 0) {
            name = argv[1];
            once = 1;
        } else {
            write(1, "$", 1);
            n = read(0, buf, 39);
            if (n > 0) {
                if (n > 39) {
                    n = 39;
                }
                buf[n] = '\0';
                trim_end(buf);
                p = skip_ws(buf);
                if (*p != '\0') {
                    name = p;
                }
            }
        }
        if (name == 0) {
            continue;
        }
        if (strcmp(name, "exit") == 0) {
            exit(0);
        }
        if (parse_line(&name, &in_path, &out_path) != 0) {
            write(1, "?", 1);
            if (once != 0) {
                fail();
            }
            continue;
        }
        pid = fork();
        if (pid < 0) {
            write(1, "?", 1);
            if (once != 0) {
                fail();
            }
            continue;
        }
        if (pid == 0) {
            if (in_path != 0 && apply_redir(in_path, 0, 0) != 0) {
                fail();
            }
            if (out_path != 0 && apply_redir(out_path, 1, 1) != 0) {
                fail();
            }
            exec(name);
            fail();
        }
        (void)wait(0);
        if (once != 0) {
            exit(0);
        }
    }
    return 1;
}
