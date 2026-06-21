#include "fs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)
struct fs_header {
	char magic[4];
	uint32_t version;
	uint32_t file_count;
	uint32_t reserved;
};

struct fs_entry {
	char name[64];
	uint32_t offset;
	uint32_t size;
};
#pragma pack(pop)

typedef struct fs_header fs_header_t;
typedef struct fs_entry fs_entry_t;

static const fs_header_t *fs_header;
static const fs_entry_t *fs_entries;
static const unsigned char *fs_image_base;
static size_t fs_entry_count;

bool fs_init(const void *image, size_t size)
{
	if (!image || size < sizeof(fs_header_t))
		return false;
	fs_header = (const fs_header_t *)image;
	if (fs_header->magic[0] != 'C' || fs_header->magic[1] != 'R' ||
	    fs_header->magic[2] != 'F' || fs_header->magic[3] != 'S')
		return false;
	if (fs_header->version != 1)
		return false;
	fs_entry_count = fs_header->file_count;
	size_t table_size = fs_entry_count * sizeof(fs_entry_t);
	size_t min_size = sizeof(fs_header_t) + table_size;
	if (size < min_size)
		return false;
	fs_entries = (const fs_entry_t *)((const unsigned char *)image + sizeof(fs_header_t));
	for (size_t i = 0; i < fs_entry_count; ++i) {
		const fs_entry_t *entry = &fs_entries[i];
		if (entry->offset + entry->size > size)
			return false;
	}
	fs_image_base = (const unsigned char *)image;
	return true;
}

static int fs_strcmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return *a - *b;
		++a;
		++b;
	}
	return *a - *b;
}

size_t fs_file_count(void)
{
	return fs_entry_count;
}

const struct fs_file *fs_file_at(size_t index)
{
	static struct fs_file file;
	if (index >= fs_entry_count)
		return 0;
	const fs_entry_t *entry = &fs_entries[index];
	file.name = entry->name;
	file.data = fs_image_base + entry->offset;
	file.size = entry->size;
	return &file;
}

const struct fs_file *fs_find(const char *name)
{
	for (size_t i = 0; i < fs_entry_count; ++i) {
		if (fs_strcmp(fs_entries[i].name, name) == 0)
			return fs_file_at(i);
	}
	return 0;
}
