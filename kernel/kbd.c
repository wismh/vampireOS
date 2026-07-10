#include "kbd.h"
#include "bio.h"
#include "fb.h"
#include "fs.h"
#include "heap.h"
#include "idt.h"
#include "io.h"
#include "mouse.h"
#include "pmm.h"
#include "sched.h"
#include "user.h"
#include "vga.h"

#include <stdint.h>

#define KBD_DATA 0x60
#define KBD_STATUS 0x64
#define KBD_STATUS_OUT 0x01
#define KBD_STATUS_AUX 0x20
#define SCAN_RELEASE 0x80
#define SCAN_EXT 0xE0
#define SCAN_LSHIFT 0x2A
#define SCAN_RSHIFT 0x36
#define SCAN_LCTRL 0x1D
#define SCAN_C 0x2E
#define SCAN_CAPS 0x3A
#define LINE_MAX 80

static const char map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.',
    [0x35] = '/', [0x39] = ' ',
};

static const char map_shift[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~',
    [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>',
    [0x35] = '?', [0x39] = ' ',
};

static int shift;
static int caps;
static int ctrl;
static int ext;
static char *line;
static unsigned line_len;
static char stdin_buf[LINE_MAX];
static unsigned stdin_len;
static unsigned stdin_have;
static int stdin_ready;
static struct interrupt_frame *kbd_irq_frame;

static int streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void puts_cur(const char *s)
{
    while (*s != '\0') {
        vga_putc(*s);
        s++;
    }
}

static void put_uint(unsigned value)
{
    char buf[10];
    int n = 0;
    unsigned v = value;

    if (v == 0) {
        vga_putc('0');
        return;
    }
    while (v > 0 && n < 10) {
        buf[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (n > 0) {
        n--;
        vga_putc(buf[n]);
    }
}

static void fb_line(int dollar)
{
    if (dollar != 0) {
        fb_prompt_line("$", stdin_buf, stdin_len);
    } else {
        fb_prompt_line("kbd>", line != 0 ? line : "", line_len);
    }
    (void)fb_present();
}

static void prompt(void)
{
    puts_cur("kbd>");
    fb_line(0);
}

static int cmd_is(const char *line, const char *cmd)
{
    while (*cmd != '\0') {
        if (*line != *cmd) {
            return 0;
        }
        line++;
        cmd++;
    }
    return *line == '\0' || *line == ' ';
}

static const char *skip_ws(const char *s)
{
    while (*s == ' ') {
        s++;
    }
    return s;
}

static void run_ls(const char *arg)
{
    unsigned here;
    char path_here[80];
    char names[80];
    const char *pwd;
    unsigned k;
    int i;
    int n;

    arg = skip_ws(arg);
    here = fs_cwd();
    pwd = fs_pwd();
    k = 0;
    while (pwd[k] != '\0' && k + 1u < 80u) {
        path_here[k] = pwd[k];
        k++;
    }
    path_here[k] = '\0';
    if (*arg != '\0' && fs_chdir(arg) != 0) {
        vga_putc('?');
        return;
    }
    n = fs_count();
    for (i = 0; i < n; i++) {
        if (i != 0) {
            vga_putc(' ');
        }
        puts_cur(fs_name(i));
        if (fs_isdir(i)) {
            vga_putc('/');
        }
    }
    if (fs_readdir(names, 80u) > 0) {
        fb_draw_text(8, 104, names);
    }
    if (*arg != '\0') {
        (void)fs_setcwd(here);
        (void)fs_setpwd(path_here);
    }
}

static void run_cat(const char *arg)
{
    const void *data;
    unsigned len;
    unsigned i;
    char buf[40];

    arg = skip_ws(arg);
    if (*arg == '\0' || fs_lookup(arg, &data, &len) != 0) {
        vga_putc('?');
        fb_draw_text(8, 56, "?    ");
        return;
    }
    for (i = 0; i < len; i++) {
        vga_putc(((const char *)data)[i]);
    }
    if (len > 0 && len < 40u) {
        for (i = 0; i < len; i++) {
            buf[i] = ((const char *)data)[i];
        }
        buf[len] = '\0';
        fb_draw_text(8, 56, buf);
    }
}

static void run_put(const char *arg)
{
    char name[40];
    unsigned n = 0;
    unsigned len = 0;
    const char *text;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        name[n++] = *arg;
        arg++;
    }
    name[n] = '\0';
    text = skip_ws(arg);
    while (text[len] != '\0') {
        len++;
    }
    if (n == 0 || len == 0 || fs_write(name, text, len) != 0) {
        vga_putc('?');
    }
}

static void run_fill(const char *arg)
{
    char name[40];
    unsigned n = 0;
    unsigned len = 0;
    unsigned i;
    char *buf;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        name[n++] = *arg;
        arg++;
    }
    name[n] = '\0';
    arg = skip_ws(arg);
    while (*arg >= '0' && *arg <= '9') {
        len = len * 10u + (unsigned)(*arg - '0');
        arg++;
    }
    if (n == 0 || len == 0 || len > 0x1000u) {
        vga_putc('?');
        return;
    }
    buf = (char *)kmalloc(len);
    if (buf == 0) {
        vga_putc('?');
        return;
    }
    for (i = 0; i < len; i++) {
        buf[i] = 'x';
    }
    if (fs_write(name, buf, len) != 0) {
        vga_putc('?');
    }
    kfree(buf);
}

static void run_trunc(const char *arg)
{
    char name[40];
    unsigned n = 0;
    unsigned len = 0;
    int have = 0;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        name[n++] = *arg;
        arg++;
    }
    name[n] = '\0';
    arg = skip_ws(arg);
    if (*arg >= '0' && *arg <= '9') {
        have = 1;
    }
    while (*arg >= '0' && *arg <= '9') {
        len = len * 10u + (unsigned)(*arg - '0');
        arg++;
    }
    if (n == 0 || have == 0 || fs_truncate(name, len) != 0) {
        vga_putc('?');
    }
}

static void run_sync(void)
{
    if (fs_sync() != 0) {
        vga_putc('?');
    }
}

static void run_devs(void)
{
    char buf[32];
    int n;

    n = bdev_list(buf, 32u);
    if (n <= 0) {
        vga_putc('?');
        return;
    }
    puts_cur(buf);
    fb_draw_text(8, 72, buf);
}

static void run_rm(const char *arg)
{
    arg = skip_ws(arg);
    if (*arg == '\0' || fs_remove(arg) != 0) {
        vga_putc('?');
    }
}

static void run_mv(const char *arg)
{
    char src[40];
    char dst[40];
    unsigned n = 0;
    unsigned m = 0;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        src[n++] = *arg;
        arg++;
    }
    src[n] = '\0';
    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && m < 39u) {
        dst[m++] = *arg;
        arg++;
    }
    dst[m] = '\0';
    if (n == 0 || m == 0 || fs_rename(src, dst) != 0) {
        vga_putc('?');
    }
}

static void run_cp(const char *arg)
{
    char src[40];
    char dst[40];
    unsigned n = 0;
    unsigned m = 0;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        src[n++] = *arg;
        arg++;
    }
    src[n] = '\0';
    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && m < 39u) {
        dst[m++] = *arg;
        arg++;
    }
    dst[m] = '\0';
    if (n == 0 || m == 0 || fs_copy(src, dst) != 0) {
        vga_putc('?');
    }
}

static void run_ln(const char *arg)
{
    char src[40];
    char dst[40];
    unsigned n = 0;
    unsigned m = 0;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        src[n++] = *arg;
        arg++;
    }
    src[n] = '\0';
    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && m < 39u) {
        dst[m++] = *arg;
        arg++;
    }
    dst[m] = '\0';
    if (n == 0 || m == 0 || fs_link(src, dst) != 0) {
        vga_putc('?');
    }
}

static void run_mkdir(const char *arg)
{
    arg = skip_ws(arg);
    if (*arg == '\0' || fs_mkdir(arg) != 0) {
        vga_putc('?');
    }
}

static void run_rmdir(const char *arg)
{
    arg = skip_ws(arg);
    if (*arg == '\0' || fs_rmdir(arg) != 0) {
        vga_putc('?');
    }
}

static void run_cd(const char *arg)
{
    arg = skip_ws(arg);
    if (*arg == '\0' || fs_chdir(arg) != 0) {
        vga_putc('?');
    }
}

static void run_pwd(void)
{
    puts_cur(fs_pwd());
}

static void run_ps(void)
{
    int i;
    int n;
    int first;
    int y;
    unsigned k;
    unsigned p;
    const char *st;
    const char *nm;
    char fb[480];

    n = sched_slots();
    first = 1;
    k = 0;
    for (i = 0; i < n; i++) {
        char id[10];
        int t;
        unsigned v;

        st = sched_slot_state_name(i);
        if (st == 0) {
            continue;
        }
        if (first == 0) {
            vga_putc(' ');
            if (k + 1u < sizeof(fb)) {
                fb[k++] = ' ';
            }
        }
        first = 0;
        put_uint((unsigned)i);
        v = (unsigned)i;
        t = 0;
        if (v == 0) {
            id[t++] = '0';
        } else {
            while (v > 0 && t < 10) {
                id[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
        }
        while (t > 0 && k + 1u < sizeof(fb)) {
            t--;
            fb[k++] = id[t];
        }
        vga_putc(' ');
        if (k + 1u < sizeof(fb)) {
            fb[k++] = ' ';
        }
        nm = sched_slot_name(i);
        if (nm != 0) {
            puts_cur(nm);
            vga_putc(' ');
            while (*nm != '\0' && k + 1u < sizeof(fb)) {
                fb[k++] = *nm;
                nm++;
            }
            if (k + 1u < sizeof(fb)) {
                fb[k++] = ' ';
            }
        }
        puts_cur(st);
        while (*st != '\0' && k + 1u < sizeof(fb)) {
            fb[k++] = *st;
            st++;
        }
        vga_putc(' ');
        if (k + 1u < sizeof(fb)) {
            fb[k++] = ' ';
        }
        v = sched_slot_ticks(i);
        put_uint(v);
        t = 0;
        if (v == 0) {
            id[t++] = '0';
        } else {
            while (v > 0 && t < 10) {
                id[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
        }
        while (t > 0 && k + 1u < sizeof(fb)) {
            t--;
            fb[k++] = id[t];
        }
    }
    if (k >= sizeof(fb)) {
        k = sizeof(fb) - 1u;
    }
    fb[k] = '\0';
    /* VBE scanout hides 80×25; wrap the same listing onto LFB rows. */
    y = 120;
    p = 0;
    while (p < k) {
        char line[40];
        int L = 0;

        while (p < k && L < 38) {
            line[L++] = fb[p++];
        }
        if (p < k) {
            int back = L;

            while (back > 8 && line[back - 1] != ' ') {
                back--;
            }
            if (back > 8 && back < L) {
                p -= (unsigned)(L - back);
                L = back;
            }
        }
        while (L > 0 && line[L - 1] == ' ') {
            L--;
        }
        line[L] = '\0';
        fb_draw_text(8, y, line);
        y += 16;
        if (y > 220) {
            break;
        }
    }
}

static void run_kill(const char *arg)
{
    unsigned pid;
    int any;

    arg = skip_ws(arg);
    pid = 0;
    any = 0;
    while (*arg >= '0' && *arg <= '9') {
        pid = pid * 10u + (unsigned)(*arg - '0');
        arg++;
        any = 1;
    }
    arg = skip_ws(arg);
    if (any == 0 || *arg != '\0' ||
        sched_kill_at((int)pid, 0, kbd_irq_frame) != 0) {
        vga_putc('?');
    }
}

static void run_prog(const char *arg)
{
    char name[40];
    unsigned n = 0;
    const char *path;

    arg = skip_ws(arg);
    while (*arg != '\0' && *arg != ' ' && n < 39u) {
        name[n++] = *arg;
        arg++;
    }
    name[n] = '\0';
    if (n == 0) {
        vga_putc('?');
        return;
    }
    path = skip_ws(arg);
    if (streq(name, "cat") || streq(name, "stat") || streq(name, "kill")) {
        if (*path == '\0' || user_run(name, path) != 0) {
            vga_putc('?');
        }
        return;
    }
    if (streq(name, "sh") || streq(name, "echo")) {
        if (user_run(name, *path != '\0' ? path : 0) != 0) {
            vga_putc('?');
        }
        return;
    }
    if (*path != '\0' || user_run(name, 0) != 0) {
        vga_putc('?');
    }
}

static void take_word(const char **s, char *out, unsigned max)
{
    unsigned n = 0;

    *s = skip_ws(*s);
    while (**s != '\0' && **s != ' ' && n + 1u < max) {
        out[n++] = **s;
        (*s)++;
    }
    out[n] = '\0';
}

/* Optional `run ` then ELF name and the rest as argv[1]. */
static int parse_side(const char *s, char *name, unsigned nmax, const char **arg)
{
    s = skip_ws(s);
    if (*s == '\0') {
        return -1;
    }
    take_word(&s, name, nmax);
    if (name[0] == '\0') {
        return -1;
    }
    if (streq(name, "run")) {
        take_word(&s, name, nmax);
        if (name[0] == '\0') {
            return -1;
        }
    }
    s = skip_ws(s);
    *arg = s;
    return 0;
}

static void run_pipe_line(char *cmd)
{
    char *bar;
    char *p;
    int bars;
    char left_name[40];
    char right_name[40];
    const char *left_arg;
    const char *right_arg;

    bars = 0;
    bar = 0;
    p = cmd;
    while (*p != '\0') {
        if (*p == '|') {
            bars++;
            bar = p;
        }
        p++;
    }
    if (bars != 1 || bar == 0) {
        vga_putc('?');
        return;
    }
    *bar = '\0';
    p = bar;
    while (p > cmd && p[-1] == ' ') {
        p--;
        *p = '\0';
    }
    if (parse_side(cmd, left_name, 40u, &left_arg) != 0 ||
        parse_side(bar + 1, right_name, 40u, &right_arg) != 0) {
        vga_putc('?');
        return;
    }
    if (*left_arg == '\0') {
        left_arg = 0;
    }
    if (*right_arg == '\0') {
        right_arg = 0;
    }
    if (user_run_pipeline(left_name, left_arg, right_name, right_arg) != 0) {
        vga_putc('?');
    }
}

static void run_redir_line(char *cmd)
{
    char *op;
    char *p;
    int n_in;
    int n_out;
    int n_app;
    char name[40];
    char file[40];
    const char *arg;
    const char *rest;
    const void *data;
    unsigned len;
    unsigned off;
    int fd;

    n_in = 0;
    n_out = 0;
    n_app = 0;
    op = 0;
    p = cmd;
    while (*p != '\0') {
        if (*p == '<') {
            n_in++;
            op = p;
        } else if (*p == '>' && p[1] == '>') {
            n_out++;
            n_app++;
            op = p;
            p++;
        } else if (*p == '>') {
            n_out++;
            op = p;
        }
        p++;
    }
    if ((n_in + n_out) != 1 || op == 0) {
        vga_putc('?');
        return;
    }
    *op = '\0';
    p = op;
    while (p > cmd && p[-1] == ' ') {
        p--;
        *p = '\0';
    }
    rest = skip_ws(n_app != 0 ? op + 2 : op + 1);
    take_word(&rest, file, 40u);
    if (file[0] == '\0') {
        vga_putc('?');
        return;
    }
    if (parse_side(cmd, name, 40u, &arg) != 0) {
        vga_putc('?');
        return;
    }
    if (*arg == '\0') {
        arg = 0;
    }
    off = 0;
    if (n_out != 0) {
        if (n_app != 0) {
            if (fs_lookup(file, &data, &len) != 0) {
                if (fs_write(file, "", 0) != 0) {
                    vga_putc('?');
                    return;
                }
                len = 0;
            }
            off = len;
        } else if (fs_write(file, "", 0) != 0) {
            vga_putc('?');
            return;
        }
    }
    if (user_run(name, arg) != 0) {
        vga_putc('?');
        return;
    }
    fd = n_in != 0 ? 0 : 1;
    if (sched_fd_bind_file(fd, file, off) != 0) {
        vga_putc('?');
    }
}

static void run_line(void)
{
    unsigned i = 0;
    unsigned j = line_len;
    const char *cmd;
    const char *scan;

    while (i < j && line[i] == ' ') {
        i++;
    }
    while (j > i && line[j - 1] == ' ') {
        j--;
    }
    line[j] = '\0';
    if (line[i] == '\0') {
        return;
    }

    cmd = line + i;
    vga_putc('\n');
    scan = cmd;
    while (*scan != '\0' && *scan != '|') {
        scan++;
    }
    if (*scan == '|') {
        run_pipe_line((char *)cmd);
        return;
    }
    scan = cmd;
    while (*scan != '\0' && *scan != '<' && *scan != '>') {
        scan++;
    }
    if (*scan == '<' || *scan == '>') {
        run_redir_line((char *)cmd);
        return;
    }
    if (streq(cmd, "help")) {
        puts_cur("help ls mem cat run put rm mv cp ln fill trunc sync devs mkdir rmdir cd pwd ps kill uptime | < > >>");
    } else if (streq(cmd, "mem")) {
        put_uint((unsigned)pmm_free_count());
    } else if (streq(cmd, "uptime")) {
        /* PIT at 100 Hz; same seconds SYS_UPTIME returns. */
        put_uint(idt_ticks() / 100u);
    } else if (streq(cmd, "pwd")) {
        run_pwd();
    } else if (streq(cmd, "ps")) {
        run_ps();
    } else if (cmd_is(cmd, "kill")) {
        run_kill(cmd + 4);
    } else if (cmd_is(cmd, "ls")) {
        run_ls(cmd + 2);
    } else if (cmd_is(cmd, "cat")) {
        run_cat(cmd + 3);
    } else if (cmd_is(cmd, "run")) {
        run_prog(cmd + 3);
    } else if (cmd_is(cmd, "put")) {
        run_put(cmd + 3);
    } else if (cmd_is(cmd, "rmdir")) {
        run_rmdir(cmd + 5);
    } else if (cmd_is(cmd, "rm")) {
        run_rm(cmd + 2);
    } else if (cmd_is(cmd, "mv")) {
        run_mv(cmd + 2);
    } else if (cmd_is(cmd, "cp")) {
        run_cp(cmd + 2);
    } else if (cmd_is(cmd, "ln")) {
        run_ln(cmd + 2);
    } else if (cmd_is(cmd, "fill")) {
        run_fill(cmd + 4);
    } else if (cmd_is(cmd, "trunc")) {
        run_trunc(cmd + 5);
    } else if (streq(cmd, "sync")) {
        run_sync();
    } else if (streq(cmd, "devs")) {
        run_devs();
    } else if (cmd_is(cmd, "mkdir")) {
        run_mkdir(cmd + 5);
    } else if (cmd_is(cmd, "cd")) {
        run_cd(cmd + 2);
    } else {
        vga_putc('?');
    }
}

void kbd_init(void)
{
    while (inb(KBD_STATUS) & KBD_STATUS_OUT) {
        (void)inb(KBD_DATA);
    }
    shift = 0;
    caps = 0;
    ctrl = 0;
    ext = 0;
    line = 0;
    line_len = 0;
    stdin_len = 0;
    stdin_have = 0;
    stdin_ready = 0;
}

void kbd_console_init(void)
{
    line = (char *)kmalloc(LINE_MAX);
    line_len = 0;
    stdin_len = 0;
    stdin_have = 0;
    stdin_ready = 0;
}

void kbd_prompt(void)
{
    prompt();
}

void kbd_stdin_prompt(void)
{
    vga_putc('$');
    fb_line(1);
}

void kbd_overlay_refresh(void)
{
    /* Draw `$` onto the shadow only; SYS_FBPRESENT copies it with the fill. */
    if (sched_kbd_waiting()) {
        fb_prompt_line("$", stdin_buf, stdin_len);
        return;
    }
    fb_prompt_line("kbd>", line != 0 ? line : "", line_len);
}

int kbd_stdin_ready(void)
{
    return stdin_ready != 0;
}

int kbd_stdin_take(void *dst, unsigned max)
{
    unsigned n;
    unsigned i;
    unsigned char *out;

    if (stdin_ready == 0) {
        return -2;
    }
    n = stdin_have;
    if (n > max) {
        n = max;
    }
    out = (unsigned char *)dst;
    if (out != 0) {
        for (i = 0; i < n; i++) {
            out[i] = (unsigned char)stdin_buf[i];
        }
    }
    stdin_ready = 0;
    stdin_have = 0;
    return (int)n;
}

/* `ps` / `help` / `run` / `kill` / `mkdir` / `mv` / `cp` / `ln` / `rm` / `fill` / `trunc` / `sync` / `devs` / plain `cat` / `ls` are kernel-only; run them at `$` without waking `sh`. */
static int line_has_meta(const char *s)
{
    while (*s != '\0') {
        if (*s == '|' || *s == '<' || *s == '>') {
            return 1;
        }
        s++;
    }
    return 0;
}

static int kbd_dollar_builtin(void)
{
    char tmp[LINE_MAX];
    unsigned i;
    char *s;
    int cat;
    int ls;

    if (stdin_len + 1u >= LINE_MAX) {
        return 0;
    }
    for (i = 0; i < stdin_len; i++) {
        tmp[i] = stdin_buf[i];
    }
    tmp[stdin_len] = '\0';
    s = tmp;
    while (*s == ' ') {
        s++;
    }
    i = 0;
    while (s[i] != '\0') {
        i++;
    }
    while (i > 0 && s[i - 1] == ' ') {
        i--;
        s[i] = '\0';
    }
    cat = cmd_is(s, "cat") != 0 && line_has_meta(s) == 0;
    ls = cmd_is(s, "ls") != 0 && line_has_meta(s) == 0;
    if (streq(s, "ps") == 0 && streq(s, "help") == 0 && streq(s, "sync") == 0 &&
        streq(s, "devs") == 0 &&
        streq(s, "mem") == 0 &&
        cmd_is(s, "mkdir") == 0 &&
        cmd_is(s, "mv") == 0 && cmd_is(s, "cp") == 0 && cmd_is(s, "ln") == 0 &&
        cmd_is(s, "rm") == 0 && cmd_is(s, "fill") == 0 &&
        cmd_is(s, "trunc") == 0 && cmd_is(s, "run") == 0 &&
        cmd_is(s, "kill") == 0 && cat == 0 && ls == 0) {
        return 0;
    }
    vga_putc('\n');
    if (streq(s, "ps") != 0) {
        run_ps();
    } else if (cmd_is(s, "kill") != 0) {
        run_kill(s + 4);
    } else if (streq(s, "help") != 0) {
        puts_cur("help ls mem cat run put rm mv cp ln fill trunc sync devs mkdir rmdir cd pwd ps kill uptime | < > >>");
    } else if (streq(s, "mem") != 0) {
        put_uint((unsigned)pmm_free_count());
    } else if (streq(s, "sync") != 0) {
        run_sync();
    } else if (streq(s, "devs") != 0) {
        run_devs();
    } else if (cmd_is(s, "mkdir") != 0) {
        run_mkdir(s + 5);
    } else if (cmd_is(s, "fill") != 0) {
        run_fill(s + 4);
    } else if (cmd_is(s, "trunc") != 0) {
        run_trunc(s + 5);
    } else if (cmd_is(s, "run") != 0) {
        run_prog(s + 3);
    } else if (ls != 0) {
        run_ls(s + 2);
    } else if (cat != 0) {
        run_cat(s + 3);
    } else if (cmd_is(s, "cp") != 0) {
        run_cp(s + 2);
    } else if (cmd_is(s, "ln") != 0) {
        run_ln(s + 2);
    } else if (cmd_is(s, "rm") != 0) {
        run_rm(s + 2);
    } else {
        run_mv(s + 2);
    }
    stdin_len = 0;
    kbd_stdin_prompt();
    return 1;
}

static void kbd_scancode(uint8_t sc)
{
    uint8_t code;
    char c;

    if (sc == SCAN_EXT) {
        ext = 1;
        return;
    }

    if (ext) {
        ext = 0;
        if (sc == SCAN_LCTRL) {
            ctrl = 1;
        } else if (sc == (SCAN_LCTRL | SCAN_RELEASE)) {
            ctrl = 0;
        }
        return;
    }

    if (sc == SCAN_LSHIFT || sc == SCAN_RSHIFT) {
        shift = 1;
        return;
    }
    if (sc == (SCAN_LSHIFT | SCAN_RELEASE) || sc == (SCAN_RSHIFT | SCAN_RELEASE)) {
        shift = 0;
        return;
    }
    if (sc == SCAN_LCTRL) {
        ctrl = 1;
        return;
    }
    if (sc == (SCAN_LCTRL | SCAN_RELEASE)) {
        ctrl = 0;
        return;
    }
    if (sc == SCAN_CAPS) {
        caps ^= 1;
        return;
    }
    if (sc & SCAN_RELEASE) {
        return;
    }

    code = sc & 0x7F;
    if (ctrl && code == SCAN_C) {
        (void)sched_signal_fg(SIGINT, 0, kbd_irq_frame);
        return;
    }
    c = shift ? map_shift[code] : map[code];
    if (c == 0) {
        return;
    }
    if (caps && c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    } else if (caps && c >= 'A' && c <= 'Z' && !shift) {
        c = (char)(c - 'A' + 'a');
    }
    kbd_feed(c);
}

void kbd_set_irq_frame(struct interrupt_frame *frame)
{
    kbd_irq_frame = frame;
}

void kbd_feed(char c)
{
    if (c == 0) {
        return;
    }
    if (c == 0x03) {
        (void)sched_signal_fg(SIGINT, 0, kbd_irq_frame);
        return;
    }

    /* Foreground `read` on fd 0: type a line, then the kernel prompt resumes. */
    if (sched_kbd_waiting()) {
        if (c == '\n') {
            if (kbd_dollar_builtin() != 0) {
                return;
            }
            if (stdin_len + 1 < LINE_MAX) {
                stdin_buf[stdin_len++] = '\n';
            }
            stdin_have = stdin_len;
            stdin_len = 0;
            stdin_ready = 1;
            sched_wake_kbd();
            vga_putc('\n');
            prompt();
            return;
        }
        if (c == '\b') {
            if (stdin_len > 0) {
                stdin_len--;
                vga_putc('\b');
                fb_line(1);
            }
            return;
        }
        if (c == '\t') {
            return;
        }
        if (stdin_len + 1 < LINE_MAX) {
            stdin_buf[stdin_len++] = c;
        }
        vga_putc(c);
        fb_line(1);
        return;
    }

    if (c == '\n') {
        if (line != 0) {
            run_line();
            line_len = 0;
        }
        vga_putc('\n');
        prompt();
        return;
    }
    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            vga_putc('\b');
            fb_line(0);
        }
        return;
    }
    if (c == '\t') {
        return;
    }
    if (line != 0 && line_len + 1 < LINE_MAX) {
        line[line_len++] = c;
    }
    vga_putc(c);
    fb_line(0);
}

void kbd_handle(struct interrupt_frame *frame)
{
    int i;

    kbd_irq_frame = frame;
    for (i = 0; i < 16; i++) {
        uint8_t st = inb(KBD_STATUS);
        uint8_t data;

        if ((st & KBD_STATUS_OUT) == 0) {
            break;
        }
        data = inb(KBD_DATA);
        if ((st & KBD_STATUS_AUX) != 0) {
            mouse_on_byte(data);
        } else {
            kbd_scancode(data);
        }
    }
}
