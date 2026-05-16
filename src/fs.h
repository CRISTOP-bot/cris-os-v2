#pragma once
#include <stddef.h>

struct FSFile {
    const char* name;
    const void* data;
    size_t size;
};

bool fs_init(const void* image, size_t size);
const FSFile* fs_find(const char* name);
const FSFile* fs_file_at(size_t index);
size_t fs_file_count();
