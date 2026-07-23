#include "gdt.h"
#include "tss.h"
#include "console.h"
#include "kstring.h"
#include <stdint.h>

#define GDT_ENTRIES 6

struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t  base_mid;
	uint8_t  access;
	uint8_t  granularity;
	uint8_t  base_high;
} __attribute__((packed));

struct gdt_entry_high {
	uint32_t base_high32;
	uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

typedef struct gdt_entry gdt_entry_t;
typedef struct gdt_ptr   gdt_ptr_t;

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static struct gdt_entry_high gdt_high[GDT_ENTRIES];
static gdt_ptr_t gdt_pointer;

void gdt_set_entry(int num, uint32_t base, uint32_t limit,
		  uint8_t access, uint8_t gran)
{
	gdt_entries[num].base_low    = base & 0xFFFF;
	gdt_entries[num].base_mid    = (base >> 16) & 0xFF;
	gdt_entries[num].base_high   = (base >> 24) & 0xFF;
	gdt_entries[num].limit_low   = limit & 0xFFFF;
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;
	gdt_entries[num].granularity |= gran & 0xF0;
	gdt_entries[num].access      = access;
	gdt_high[num].base_high32 = 0;
	gdt_high[num].reserved = 0;
}

void gdt_set_tss(int num, uint64_t base, uint32_t limit)
{
	gdt_entries[num].base_low    = base & 0xFFFF;
	gdt_entries[num].base_mid    = (base >> 16) & 0xFF;
	gdt_entries[num].base_high   = (base >> 24) & 0xFF;
	gdt_entries[num].limit_low   = limit & 0xFFFF;
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;
	gdt_entries[num].granularity |= 0xA0;
	gdt_entries[num].access      = 0x89;
	gdt_high[num].base_high32 = (uint32_t)(base >> 32);
	gdt_high[num].reserved = 0;
}

void gdt_init(void)
{
	gdt_set_entry(0, 0, 0, 0, 0);
	gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);
	gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
	gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xAF);
	gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
	gdt_set_tss(5, 0, 0);

	gdt_pointer.limit = sizeof(gdt_entry_t) * GDT_ENTRIES - 1;
	gdt_pointer.base  = (uint64_t)&gdt_entries;

	gdt_load((uint64_t)&gdt_pointer);
	console_print("[ OK ] GDT initialized (64-bit flat mode)\n");
}

void gdt_reload(void)
{
	gdt_load((uint64_t)&gdt_pointer);
}
