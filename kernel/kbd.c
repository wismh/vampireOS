#include "kbd.h"
#include "fs.h"
#include "heap.h"
#include "io.h"
#include "pmm.h"
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
    int i;
    int n;

    arg = skip_ws(arg);
    here = fs_cwd();
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

static void run_mkdir(const char *arg)
{
    arg = skip_ws(arg);
    if (*arg == '\0' || fs_mkdir(arg) != 0) {
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

static void run_prog(const char *arg)
{
    arg = skip_ws(arg);
    if (user_run(arg) != 0) {
        vga_putc('?');
    }
}

static void run_line(void)
{
    unsigned i = 0;
    unsigned j = line_len;
    const char *cmd;

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
    if (streq(cmd, "help")) {
        puts_cur("help ls mem cat run put rm fill mkdir cd");
    } else if (streq(cmd, "mem")) {
        put_uint((unsigned)pmm_free_count());
    } else if (cmd_is(cmd, "ls")) {
        run_ls(cmd + 2);
    } else if (cmd_is(cmd, "cat")) {
        run_cat(cmd + 3);
    } else if (cmd_is(cmd, "run")) {
        run_prog(cmd + 3);
    } else if (cmd_is(cmd, "put")) {
        run_put(cmd + 3);
    } else if (cmd_is(cmd, "rm")) {
        run_rm(cmd + 2);
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
