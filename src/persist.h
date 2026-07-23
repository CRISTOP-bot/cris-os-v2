#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>
#include <stdbool.h>

#define PERSIST_MAGIC 0x50455253
#define PERSIST_MAX_ENTRIES 128
#define PERSIST_NAME_SIZE 64

struct persist_entry {
	char name[PERSIST_NAME_SIZE];
	uint32_t size;
	uint32_t offset;
};

struct persist_header {
	uint32_t magic;
	uint32_t version;
	uint32_t entry_count;
	uint32_t data_size;
};

bool persist_init(void);
bool persist_save_file(const char *name, const void *data, uint32_t size);
bool persist_load_file(const char *name, void *buf, uint32_t max_size, uint32_t *out_size);
bool persist_delete_file(const char *name);
bool persist_file_exists(const char *name);
uint32_t persist_get_total_size(void);

#endif
