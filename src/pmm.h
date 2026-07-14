#ifndef PMM_H
#define PMM_H

#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

void pmm_init(unsigned long mem_lower, unsigned long mem_upper);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
unsigned long pmm_get_total_pages(void);
unsigned long pmm_get_free_pages(void);
unsigned long pmm_get_used_pages(void);
unsigned long pmm_get_total_bytes(void);
unsigned long pmm_get_free_bytes(void);

#endif /* PMM_H */
