#pragma once

void fs_init(void);
int fs_count(void);
const char *fs_name(int i);
int fs_lookup(const char *name, const void **data, unsigned *len);
