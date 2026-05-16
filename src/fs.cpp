#include "fs.h"
#include <stddef.h>
#include <stdint.h>

#pragma pack(push, 1)
struct FSHeader {
    char magic[4];
    uint32_t version;
    uint32_t file_count;
    uint32_t reserved;
};

struct FSEntry {
    char name[16];
    uint32_t offset;
    uint32_t size;
};
#pragma pack(pop)

static const FSHeader* header = 0;
static const FSEntry* entries = 0;
static const unsigned char* image_base = 0;
static size_t entry_count = 0;

bool fs_init(const void* image, size_t size) {
    if (!image || size < sizeof(FSHeader)) return false;
    header = (const FSHeader*)image;
    if (header->magic[0] != 'C' || header->magic[1] != 'R' || header->magic[2] != 'F' || header->magic[3] != 'S') return false;
    if (header->version != 1) return false;
    entry_count = header->file_count;
    size_t table_size = entry_count * sizeof(FSEntry);
    size_t min_size = sizeof(FSHeader) + table_size;
    if (size < min_size) return false;
    entries = (const FSEntry*)((const unsigned char*)image + sizeof(FSHeader));
    for (size_t i = 0; i < entry_count; ++i) {
        const FSEntry* entry = &entries[i];
        if (entry->offset + entry->size > size) return false;
    }
    image_base = (const unsigned char*)image;
    return true;
}

static int kstrcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        ++a; ++b;
    }
    return *a - *b;
}

size_t fs_file_count() {
    return entry_count;
}

const FSFile* fs_file_at(size_t index) {
    static FSFile file;
    if (index >= entry_count) return 0;
    const FSEntry* entry = &entries[index];
    file.name = entry->name;
    file.data = image_base + entry->offset;
    file.size = entry->size;
    return &file;
}

const FSFile* fs_find(const char* name) {
    for (size_t i = 0; i < entry_count; ++i) {
        if (kstrcmp(entries[i].name, name) == 0) {
            return fs_file_at(i);
        }
    }
    return 0;
}
