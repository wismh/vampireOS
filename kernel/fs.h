#pragma once

int fs_init(int row);
int fs_count(void);
const char *fs_name(int i);
int fs_lookup(const char *name, const void **data, unsigned *len);
int fs_write(const char *name, const void *data, unsigned len);
int fs_remove(const char *name);
int fs_isdir(int i);
int fs_mkdir(const char *name);
int fs_rmdir(const char *name);
int fs_chdir(const char *name);
unsigned fs_cwd(void);
int fs_setcwd(unsigned cl);
const char *fs_pwd(void);
int fs_setpwd(const char *path);
