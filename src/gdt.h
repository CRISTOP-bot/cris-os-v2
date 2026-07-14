#ifndef NUCLEOS_GDT_H
#define NUCLEOS_GDT_H

#include <stdint.h>

void gdt_init(void);
void gdt_load(uint64_t gdtr);

#endif
