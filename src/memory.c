#include "memory.h"
#include "pmm.h"
#include "kstring.h"
#include "console.h"

#define BLOCK_MAGIC 0xDEADBEEF

struct block_header {
	unsigned int magic;
	size_t size;
	int free;
	struct block_header *next;
	struct block_header *prev;
};

static struct block_header *heap_start;
static struct block_header *heap_end;
static size_t heap_total;
static size_t heap_used_blocks;
static void *heap_base;
static int heap_page_count;

static size_t align_up(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

void memory_init(void)
{
	heap_start = 0;
	heap_end = 0;
	heap_total = 0;
	heap_used_blocks = 0;
	heap_page_count = 0;
	heap_base = 0;

	heap_page_count = 16;
	heap_base = pmm_alloc_page();
	if (!heap_base) {
		console_print("[MEM] WARNING: No page for heap\n");
		return;
	}
	heap_page_count = 1;
	heap_total = PAGE_SIZE;

	heap_start = (struct block_header *)heap_base;
	heap_start->magic = BLOCK_MAGIC;
	heap_start->size = heap_total - sizeof(struct block_header);
	heap_start->free = 1;
	heap_start->next = 0;
	heap_start->prev = 0;
	heap_end = heap_start;

	char buf[32];
	console_print("[MEM] Heap: ");
	kitoa(heap_total / 1024, buf, sizeof(buf));
	console_print(buf);
	console_print(" KB (1 page)\n");
}

static struct block_header *find_free_block(size_t size)
{
	struct block_header *cur = heap_start;
	while (cur) {
		if (cur->free && cur->magic == BLOCK_MAGIC && cur->size >= size)
			return cur;
		cur = cur->next;
	}
	return 0;
}

static void split_block(struct block_header *block, size_t size)
{
	if (block->size < size + sizeof(struct block_header) + 16)
		return;
	struct block_header *new_block =
		(struct block_header *)((char *)block + sizeof(struct block_header) + size);
	new_block->magic = BLOCK_MAGIC;
	new_block->size = block->size - size - sizeof(struct block_header);
	new_block->free = 1;
	new_block->next = block->next;
	new_block->prev = block;
	if (block->next)
		block->next->prev = new_block;
	block->next = new_block;
	block->size = size;
	if (heap_end == block)
		heap_end = new_block;
}

static void merge_free(struct block_header *block)
{
	while (block->next && block->next->free && block->next->magic == BLOCK_MAGIC) {
		block->size += sizeof(struct block_header) + block->next->size;
		block->next = block->next->next;
		if (block->next)
			block->next->prev = block;
	}
	if (block->prev && block->prev->free && block->prev->magic == BLOCK_MAGIC) {
		block->prev->size += sizeof(struct block_header) + block->size;
		block->prev->next = block->next;
		if (block->next)
			block->next->prev = block->prev;
	}
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return 0;
	size = align_up(size, 8);
	if (!heap_start)
		return 0;
	struct block_header *block = find_free_block(size);
	if (!block)
		return 0;
	split_block(block, size);
	block->free = 0;
	heap_used_blocks++;
	return (char *)block + sizeof(struct block_header);
}

void *kmalloc_aligned(size_t size)
{
	size_t total = align_up(size, PAGE_SIZE) + 2 * PAGE_SIZE;
	void *raw = kmalloc(total);
	if (!raw)
		return 0;
	unsigned long addr = (unsigned long)raw;
	unsigned long aligned = align_up(addr, PAGE_SIZE);
	return (void *)aligned;
}

void *kcalloc(size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > (size_t)-1 / nmemb)
		return 0;
	size_t total = nmemb * size;
	void *p = kmalloc(total);
	if (p)
		kmemset(p, 0, total);
	return p;
}

void *krealloc(void *ptr, size_t size)
{
	if (!ptr)
		return kmalloc(size);
	if (size == 0) {
		kfree(ptr);
		return 0;
	}
	struct block_header *block = (struct block_header *)((char *)ptr - sizeof(struct block_header));
	if (block->magic != BLOCK_MAGIC || block->free)
		return 0;
	if (block->size >= size)
		return ptr;
	void *new_ptr = kmalloc(size);
	if (new_ptr) {
		kmemcpy(new_ptr, ptr, block->size);
		kfree(ptr);
	}
	return new_ptr;
}

void kfree(void *p)
{
	if (!p)
		return;
	struct block_header *block = (struct block_header *)((char *)p - sizeof(struct block_header));
	if (block->magic != BLOCK_MAGIC)
		return;
	if (block->free)
		return;
	block->free = 1;
	heap_used_blocks--;
	merge_free(block);
}
