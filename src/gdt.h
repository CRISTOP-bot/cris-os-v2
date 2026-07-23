#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);
void gdt_load(uint64_t gdtr);
void gdt_reload(void);
void gdt_set_tss(int num, uint64_t base, uint32_t limit);

#endif
