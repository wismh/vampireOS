#pragma once

int fs_init(int row);
int fs_count(void);
const char *fs_name(int i);
int fs_lookup(const char *name, const void **data, unsigned *len);
int fs_write(const char *name, const void *data, unsigned len);
