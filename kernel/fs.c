#include "fs.h"

#define FS_MAX 2

struct fs_file {
    const char *name;
    const char *data;
};

static struct fs_file files[FS_MAX];
static int file_count;

void fs_init(void)
{
    files[0].name = "hello";
    files[0].data = "blood";
    files[1].name = "motd";
    files[1].data = "night";
    file_count = 2;
}

int fs_count(void)
{
    return file_count;
}

const char *fs_name(int i)
{
    if (i < 0 || i >= file_count) {
        return 0;
    }
    return files[i].name;
}

int fs_lookup(const char *name, const char **data)
{
    int i;
    const char *a;
    const char *b;

    if (name == 0 || data == 0) {
        return -1;
    }
    for (i = 0; i < file_count; i++) {
        a = name;
        b = files[i].name;
        while (*a != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            *data = files[i].data;
            return 0;
        }
    }
    return -1;
}
