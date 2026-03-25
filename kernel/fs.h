#pragma once

int fs_init(int row);
int fs_count(void);
const char *fs_name(int i);
/* Pack cwd names into dst (space-separated, dirs with '/'). Returns bytes. */
int fs_readdir(char *dst, unsigned max);
int fs_lookup(const char *name, const void **data, unsigned *len);
/* Packed user ints: size, first cluster, is-dir. */
int fs_stat(const char *name, unsigned *size, unsigned *cluster, unsigned *is_dir);
int fs_write(const char *name, const void *data, unsigned len);
int fs_remove(const char *name);
int fs_rename(const char *src, const char *dst);
int fs_copy(const char *src, const char *dst);
int fs_isdir(int i);
int fs_mkdir(const char *name);
int fs_rmdir(const char *name);
int fs_chdir(const char *name);
unsigned fs_cwd(void);
int fs_setcwd(unsigned cl);
const char *fs_pwd(void);
int fs_setpwd(const char *path);
