#ifndef FS_H
#define FS_H
#include <stdbool.h>
#include <stddef.h>

struct fs_file {
    const char *name;
    const void *data;
    size_t size;
};

bool fs_init(const void *image, size_t size);
const struct fs_file *fs_find(const char *name);
const struct fs_file *fs_file_at(size_t index);
size_t fs_file_count(void);

#endif /* FS_H */
