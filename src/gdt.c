#include "gdt.h"
#include "console.h"
#include <stdint.h>

#define GDT_ENTRIES 5

struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t  base_mid;
	uint8_t  access;
	uint8_t  granularity;
	uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

typedef struct gdt_entry gdt_entry_t;
typedef struct gdt_ptr   gdt_ptr_t;

static gdt_entry_t gdt_entries[GDT_ENTRIES];

static void gdt_set_entry(int num, uint32_t base, uint32_t limit,
			  uint8_t access, uint8_t gran)
{
	gdt_entries[num].base_low    = base & 0xFFFF;
	gdt_entries[num].base_mid    = (base >> 16) & 0xFF;
	gdt_entries[num].base_high   = (base >> 24) & 0xFF;
	gdt_entries[num].limit_low   = limit & 0xFFFF;
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;
	gdt_entries[num].granularity |= gran & 0xF0;
	gdt_entries[num].access      = access;
}

void gdt_init(void)
{
	gdt_set_entry(0, 0, 0, 0, 0);
	gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);
	gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
	gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xAF);
	gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);

	gdt_ptr_t gp;
	gp.limit = sizeof(gdt_entry_t) * GDT_ENTRIES - 1;
	gp.base  = (uint64_t)&gdt_entries;

	gdt_load((uint64_t)&gp);
	console_print("[ OK ] GDT initialized (64-bit flat mode)\n");
}
