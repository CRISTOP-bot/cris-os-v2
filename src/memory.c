#include "memory.h"

static unsigned char heap_area[64*1024];
static size_t heap_used = 0;

void* kmalloc(size_t size) {
    if (size == 0) return 0;
    // simple 4-byte align
    size = (size + 3) & ~3;
    if (heap_used + size > sizeof(heap_area)) return 0;
    void* p = &heap_area[heap_used];
    heap_used += size;
    return p;
}

void kfree(void* p) {
    // no-op simple allocator
    (void)p;
}
