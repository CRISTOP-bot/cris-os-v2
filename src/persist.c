#include "persist.h"
#include "kstring.h"
#include "console.h"
#include "pmm.h"

static uint8_t *persist_base;
static uint32_t persist_total;
static struct persist_header *header;
static struct persist_entry *entries;
static uint8_t *data_area;

bool persist_init(void)
{
	persist_total = PAGE_SIZE * 4;
	persist_base = (uint8_t *)pmm_alloc_page();
	if (!persist_base) {
		console_print("[PERSIST] Failed to allocate page\n");
		return false;
	}

	header = (struct persist_header *)persist_base;
	entries = (struct persist_entry *)(persist_base + sizeof(struct persist_header));
	data_area = persist_base + sizeof(struct persist_header) +
		    PERSIST_MAX_ENTRIES * sizeof(struct persist_entry);

	if (header->magic == PERSIST_MAGIC) {
		char buf[16];
		kitoa(header->entry_count, buf, sizeof(buf));
		console_print("[PERSIST] Restored ");
		console_print(buf);
		console_print(" file(s)\n");
		return true;
	}

	header->magic = PERSIST_MAGIC;
	header->version = 1;
	header->entry_count = 0;
	header->data_size = 0;

	return true;
}

static struct persist_entry *find_entry(const char *name)
{
	for (uint32_t i = 0; i < header->entry_count; ++i) {
		if (kstrcmp(entries[i].name, name) == 0)
			return &entries[i];
	}
	return 0;
}

bool persist_save_file(const char *name, const void *data, uint32_t size)
{
	if (!name || !data || size == 0)
		return false;

	struct persist_entry *existing = find_entry(name);
	if (existing) {
		uint32_t new_offset = header->data_size;
		if (new_offset + size > persist_total - sizeof(struct persist_header) -
		    PERSIST_MAX_ENTRIES * sizeof(struct persist_entry))
			return false;
		kmemcpy(data_area + new_offset, data, size);
		existing->offset = new_offset;
		existing->size = size;
		header->data_size = new_offset + size;
		return true;
	}

	if (header->entry_count >= PERSIST_MAX_ENTRIES)
		return false;

	uint32_t new_offset = header->data_size;
	if (new_offset + size > persist_total - sizeof(struct persist_header) -
	    PERSIST_MAX_ENTRIES * sizeof(struct persist_entry))
		return false;

	kstrcpy(entries[header->entry_count].name, name, PERSIST_NAME_SIZE);
	entries[header->entry_count].offset = new_offset;
	entries[header->entry_count].size = size;
	header->entry_count++;
	header->data_size = new_offset + size;

	kmemcpy(data_area + new_offset, data, size);
	return true;
}

bool persist_load_file(const char *name, void *buf, uint32_t max_size, uint32_t *out_size)
{
	struct persist_entry *entry = find_entry(name);
	if (!entry)
		return false;
	if (entry->size > max_size)
		return false;
	kmemcpy(buf, data_area + entry->offset, entry->size);
	if (out_size)
		*out_size = entry->size;
	return true;
}

bool persist_delete_file(const char *name)
{
	struct persist_entry *entry = find_entry(name);
	if (!entry)
		return false;
	uint32_t idx = (uint32_t)(entry - entries);
	for (uint32_t i = idx; i < header->entry_count - 1; ++i)
		kmemcpy(&entries[i], &entries[i + 1], sizeof(struct persist_entry));
	header->entry_count--;
	return true;
}

bool persist_file_exists(const char *name)
{
	return find_entry(name) != 0;
}

uint32_t persist_get_total_size(void)
{
	return header->data_size;
}
