#include "kbd.h"
#include "fs.h"
#include "heap.h"
#include "io.h"
#include "pmm.h"
#include "sched.h"
#include "user.h"
#include "vga.h"

#include <stdint.h>

#define KBD_DATA 0x60
#define KBD_STATUS 0x64
#define KBD_STATUS_OUT 0x01
#define SCAN_RELEASE 0x80
#define SCAN_EXT 0xE0
#define SCAN_LSHIFT 0x2A
#define SCAN_RSHIFT 0x36
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
static int ext;
static char *line;
static unsigned line_len;

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

static void prompt(void)
{
    puts_cur("kbd>");
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

    arg = skip_ws(arg);
    if (*arg == '\0' || fs_lookup(arg, &data, &len) != 0) {
        vga_putc('?');
        return;
    }
    for (i = 0; i < len; i++) {
        vga_putc(((const char *)data)[i]);
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
    const char *st;

    n = sched_slots();
    first = 1;
    for (i = 0; i < n; i++) {
        st = sched_slot_state_name(i);
        if (st == 0) {
            continue;
        }
        if (first == 0) {
            vga_putc(' ');
        }
        first = 0;
        put_uint((unsigned)i);
        vga_putc(' ');
        puts_cur(st);
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
    if (streq(name, "cat")) {
        if (*path == '\0' || user_run("cat", path) != 0) {
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
    if (streq(cmd, "help")) {
        puts_cur("help ls mem cat run put rm mv fill mkdir rmdir cd pwd ps |");
    } else if (streq(cmd, "mem")) {
        put_uint((unsigned)pmm_free_count());
    } else if (streq(cmd, "pwd")) {
        run_pwd();
    } else if (streq(cmd, "ps")) {
        run_ps();
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
    } else if (cmd_is(cmd, "fill")) {
        run_fill(cmd + 4);
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
    ext = 0;
    line = 0;
    line_len = 0;
}

void kbd_console_init(void)
{
    line = (char *)kmalloc(LINE_MAX);
    line_len = 0;
    prompt();
}

void kbd_handle(void)
{
    uint8_t sc = inb(KBD_DATA);
    uint8_t code;
    char c;

    if (sc == SCAN_EXT) {
        ext = 1;
        return;
    }

    if (ext) {
        ext = 0;
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
    if (sc == SCAN_CAPS) {
        caps ^= 1;
        return;
    }
    if (sc & SCAN_RELEASE) {
        return;
    }

    code = sc & 0x7F;
    c = shift ? map_shift[code] : map[code];
    if (c == 0) {
        return;
    }
    if (caps && c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    } else if (caps && c >= 'A' && c <= 'Z' && !shift) {
        c = (char)(c - 'A' + 'a');
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
}
