#include "tss.h"
#include "gdt.h"
#include "console.h"
#include "kstring.h"

static struct tss_entry tss;

void tss_init(void)
{
	kmemset(&tss, 0, sizeof(tss));

	tss.iopb_offset = sizeof(struct tss_entry);
	tss.rsp0 = 0;

	gdt_set_tss(5, (uint64_t)&tss, sizeof(struct tss_entry) - 1);
	gdt_reload();

	tss_load(0x28);

	console_print("[ OK ] TSS initialized\n");
}

void tss_set_rsp0(uint64_t rsp0)
{
	tss.rsp0 = rsp0;
}
