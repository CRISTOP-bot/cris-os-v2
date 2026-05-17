#pragma once

#include <stddef.h>
#include <stdbool.h>

bool vfs_init();
const char* vfs_pwd();
bool vfs_cd(const char* path);
bool vfs_list(const char* path);
bool vfs_cat(const char* path);
bool vfs_mkdir(const char* path);
bool vfs_touch(const char* path);
bool vfs_remove(const char* path);
bool vfs_rmdir(const char* path);
bool vfs_cp(const char* src, const char* dst);
bool vfs_mv(const char* src, const char* dst);
bool vfs_write(const char* path, const char* content, size_t length);
const void* vfs_read(const char* path);
size_t vfs_get_size(const char* path);
bool vfs_exists(const char* path);
