#ifndef CRISOS_GDT_H
#define CRISOS_GDT_H

void gdt_init(void);
void gdt_load(unsigned int gdtr);

#endif
