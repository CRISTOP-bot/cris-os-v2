#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdbool.h>

int vfs_init(void);
const char *vfs_pwd(void);
int vfs_cd(const char *path);
int vfs_list(const char *path);
int vfs_cat(const char *path);
int vfs_mkdir(const char *path);
int vfs_touch(const char *path);
int vfs_remove(const char *path);
int vfs_rmdir(const char *path);
int vfs_cp(const char *src, const char *dst);
int vfs_mv(const char *src, const char *dst);
int vfs_write(const char *path, const char *content, size_t length);
const void *vfs_read(const char *path);
size_t vfs_get_size(const char *path);
int vfs_exists(const char *path);

#endif /* VFS_H */
