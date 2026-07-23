#include "pmm.h"
#include "kstring.h"
#include "console.h"

#define MAX_PAGES (1024 * 1024)
#define BITMAP_SIZE (MAX_PAGES / 32)

extern char _kernel_start[];
extern char _kernel_end[];

static unsigned int pmm_bitmap[BITMAP_SIZE];
static unsigned long pmm_total_pages;
static unsigned long pmm_free_count;

static inline void bitmap_set(unsigned long page)
{
	pmm_bitmap[page / 32] |= (1U << (page % 32));
}

static inline void bitmap_clear(unsigned long page)
{
	pmm_bitmap[page / 32] &= ~(1U << (page % 32));
}

static inline int bitmap_test(unsigned long page)
{
	return (pmm_bitmap[page / 32] >> (page % 32)) & 1;
}

void pmm_init(unsigned long mem_lower, unsigned long mem_upper)
{
	(void)mem_lower;
	unsigned long total_ram = 1024 + mem_upper;
	pmm_total_pages = total_ram / PAGE_SIZE;
	if (pmm_total_pages > MAX_PAGES)
		pmm_total_pages = MAX_PAGES;

	pmm_free_count = 0;

	kmemset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));

	unsigned long kernel_end_page = ((unsigned long)_kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;

	for (unsigned long i = 0; i < kernel_end_page; ++i)
		bitmap_set(i);

	for (unsigned long i = 0x9FC00 / PAGE_SIZE; i < 0x100000 / PAGE_SIZE; ++i)
		bitmap_set(i);

	for (unsigned long i = kernel_end_page; i < pmm_total_pages; ++i) {
		if (bitmap_test(i)) {
			bitmap_clear(i);
			pmm_free_count++;
		}
	}

	char buf[32];
	console_print("[PMM] Total RAM: ");
	kitoa(total_ram / 1024, buf, sizeof(buf));
	console_print(buf);
	console_print(" MB, ");
	kitoa(pmm_total_pages, buf, sizeof(buf));
	console_print(buf);
	console_print(" pages (4KB)\n");
}

void *pmm_alloc_page(void)
{
	for (unsigned long i = 0; i < pmm_total_pages; ++i) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			pmm_free_count--;
			void *page = (void *)(i * PAGE_SIZE);
			kmemset(page, 0, PAGE_SIZE);
			return page;
		}
	}
	return 0;
}

void pmm_free_page(void *page)
{
	if (!page)
		return;
	unsigned long addr = (unsigned long)page;
	unsigned long page_idx = addr / PAGE_SIZE;
	if (page_idx >= pmm_total_pages)
		return;
	if (bitmap_test(page_idx)) {
		bitmap_clear(page_idx);
		pmm_free_count++;
	}
}

unsigned long pmm_get_total_pages(void)
{
	return pmm_total_pages;
}

unsigned long pmm_get_free_pages(void)
{
	return pmm_free_count;
}

unsigned long pmm_get_used_pages(void)
{
	return pmm_total_pages - pmm_free_count;
}

unsigned long pmm_get_total_bytes(void)
{
	return pmm_total_pages * PAGE_SIZE;
}

unsigned long pmm_get_free_bytes(void)
{
	return pmm_free_count * PAGE_SIZE;
}
