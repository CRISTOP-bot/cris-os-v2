#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

void memory_init(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *p);

#endif /* MEMORY_H */
