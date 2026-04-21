/* User shell: nested `|` plus one `<` or `>`; fork/exec keeps console fds. */
long write(int fd, const void *buf, unsigned long n);
long read(int fd, void *buf, unsigned long n);
long exec(const char *path);
long open(const char *path, long flags);
long dup2(int oldfd, int newfd);
long close(int fd);
long fork(void);
long wait(long pid);
long pipe(int fd[2]);
void exit(int code);
int strcmp(const char *a, const char *b);

enum { CMD_MAX = 4 };

static void fail(void)
{
    write(2, "?", 1);
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

/* Left-to-right `a | b | c`. Empty `|` or more than CMD_MAX stages fail. */
static int split_pipes(char *line, char **cmds, int max)
{
    int n;
    char *p;
    char *start;

    n = 0;
    p = line;
    for (;;) {
        start = skip_ws(p);
        if (*start == '\0' || *start == '|') {
            return -1;
        }
        if (n >= max) {
            return -1;
        }
        cmds[n] = start;
        n++;
        while (*p != '\0' && *p != '|') {
            p++;
        }
        if (*p == '\0') {
            trim_end(cmds[n - 1]);
            if (cmds[n - 1][0] == '\0') {
                return -1;
            }
            return n;
        }
        *p = '\0';
        trim_end(cmds[n - 1]);
        if (cmds[n - 1][0] == '\0') {
            return -1;
        }
        p++;
    }
}

static void drop(int fd)
{
    if (fd >= 0) {
        (void)close(fd);
    }
}

static void child_exec(char *name, char *in_path, char *out_path, int in_fd,
                       int out_fd, int pr, int pw)
{
    if (in_fd >= 0 && dup2(in_fd, 0) < 0) {
        fail();
    }
    if (out_fd >= 0 && dup2(out_fd, 1) < 0) {
        fail();
    }
    if (in_fd >= 0 && in_fd != 0) {
        drop(in_fd);
    }
    if (out_fd >= 0 && out_fd != 1) {
        drop(out_fd);
    }
    if (pr >= 0 && pr != 0 && pr != 1) {
        drop(pr);
    }
    if (pw >= 0 && pw != 0 && pw != 1) {
        drop(pw);
    }
    if (in_path != 0 && apply_redir(in_path, 0, 0) != 0) {
        fail();
    }
    if (out_path != 0 && apply_redir(out_path, 1, 1) != 0) {
        fail();
    }
    exec(name);
    fail();
}

static int run_cmds(char **names, char **ins, char **outs, int n)
{
    int i;
    int p[2];
    int in_fd;
    int pr;
    int pw;
    int out_fd;
    long pid;
    int started;

    in_fd = -1;
    started = 0;
    for (i = 0; i < n; i++) {
        pr = -1;
        pw = -1;
        out_fd = -1;
        if (i + 1 < n) {
            if (pipe(p) != 0) {
                drop(in_fd);
                for (i = 0; i < started; i++) {
                    (void)wait(0);
                }
                return -1;
            }
            pr = p[0];
            pw = p[1];
            out_fd = pw;
        }
        pid = fork();
        if (pid < 0) {
            drop(in_fd);
            drop(pr);
            drop(pw);
            for (i = 0; i < started; i++) {
                (void)wait(0);
            }
            return -1;
        }
        if (pid == 0) {
            child_exec(names[i], ins[i], outs[i], in_fd, out_fd, pr, pw);
        }
        started++;
        drop(in_fd);
        drop(pw);
        in_fd = pr;
    }
    drop(in_fd);
    for (i = 0; i < started; i++) {
        (void)wait(0);
    }
    return 0;
}

int main(int argc, char **argv)
{
    char buf[40];
    char *line;
    char *p;
    char *cmds[CMD_MAX];
    char *names[CMD_MAX];
    char *ins[CMD_MAX];
    char *outs[CMD_MAX];
    long nread;
    int n;
    int i;
    int once;

    once = 0;
    for (;;) {
        line = 0;
        if (once == 0 && argc >= 2 && argv != 0 && argv[1] != 0 &&
            strcmp(argv[1], "") != 0) {
            line = argv[1];
            once = 1;
        } else {
            write(1, "$", 1);
            nread = read(0, buf, 39);
            if (nread > 0) {
                if (nread > 39) {
                    nread = 39;
                }
                buf[nread] = '\0';
                trim_end(buf);
                p = skip_ws(buf);
                if (*p != '\0') {
                    line = p;
                }
            }
        }
        if (line == 0) {
            continue;
        }
        if (strcmp(line, "exit") == 0) {
            exit(0);
        }
        n = split_pipes(line, cmds, CMD_MAX);
        if (n < 1) {
            write(1, "?", 1);
            if (once != 0) {
                fail();
            }
            continue;
        }
        for (i = 0; i < n; i++) {
            names[i] = cmds[i];
            ins[i] = 0;
            outs[i] = 0;
            if (parse_line(&names[i], &ins[i], &outs[i]) != 0) {
                n = -1;
                break;
            }
        }
        if (n < 1) {
            write(1, "?", 1);
            if (once != 0) {
                fail();
            }
            continue;
        }
        if (run_cmds(names, ins, outs, n) != 0) {
            write(1, "?", 1);
            if (once != 0) {
                fail();
            }
            continue;
        }
        if (once != 0) {
            exit(0);
        }
    }
    return 1;
}
