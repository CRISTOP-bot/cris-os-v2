#include "vmm.h"
#include "pmm.h"
#include "kstring.h"
#include "console.h"
#include <stdint.h>

#define PDP_ENTRIES  512

#define PRESENT  (1ULL << 0)
#define WRITE    (1ULL << 1)
#define USER     (1ULL << 2)
#define PS       (1ULL << 7)
#define NX       (1ULL << 63)

static uint64_t pdp_table[PDP_ENTRIES]   __attribute__((aligned(4096)));

static inline void invlpg(unsigned long addr)
{
	__asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void vmm_init(unsigned long mem_lower, unsigned long mem_upper)
{
	(void)mem_lower;
	(void)mem_upper;

	unsigned long total_ram = 1024 + mem_upper;
	unsigned long map_end = total_ram;
	if (map_end > 16 * 1024 * 1024)
		map_end = 16 * 1024 * 1024;

	char buf[32];
	console_print("[VMM] Using boot.S identity map, mapped ");
	kitoa(map_end / (1024 * 1024), buf, sizeof(buf));
	console_print(buf);
	console_print(" MB\n");

	(void)map_end;
}

void vmm_map_page(unsigned long phys, unsigned long virt, unsigned int flags)
{
	unsigned int pml4_idx = (virt >> 39) & 0x1FF;
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	if (pml4_idx != 0)
		return;

	uint64_t entry_flags = (uint64_t)(flags & 0x1F) | PRESENT;

	if (!(pdp_table[pdp_idx] & PRESENT)) {
		uint64_t *new_pd = (uint64_t *)pmm_alloc_page();
		if (!new_pd) return;
		kmemset(new_pd, 0, PAGE_SIZE);
		pdp_table[pdp_idx] = (uint64_t)new_pd | entry_flags;
	}

	uint64_t *pd = (uint64_t *)(pdp_table[pdp_idx] & ~0xFFFULL);

	if (!(pd[pd_idx] & PRESENT)) {
		uint64_t *new_pt = (uint64_t *)pmm_alloc_page();
		if (!new_pt) return;
		kmemset(new_pt, 0, PAGE_SIZE);
		pd[pd_idx] = (uint64_t)new_pt | entry_flags;
	}

	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	pt[pt_idx] = (phys & ~0xFFFULL) | entry_flags;
	invlpg(virt);
}

void vmm_unmap_page(unsigned long virt)
{
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	if (!(pdp_table[pdp_idx] & PRESENT)) return;
	uint64_t *pd = (uint64_t *)(pdp_table[pdp_idx] & ~0xFFFULL);
	if (!(pd[pd_idx] & PRESENT)) return;
	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	pt[pt_idx] = 0;
	invlpg(virt);
}

bool vmm_is_page_mapped(unsigned long virt)
{
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	if (!(pdp_table[pdp_idx] & PRESENT)) return false;
	uint64_t *pd = (uint64_t *)(pdp_table[pdp_idx] & ~0xFFFULL);
	if (!(pd[pd_idx] & PRESENT)) return false;
	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	return (pt[pt_idx] & PRESENT) != 0;
}

unsigned long vmm_get_phys(unsigned long virt)
{
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	if (!(pdp_table[pdp_idx] & PRESENT)) return 0;
	uint64_t *pd = (uint64_t *)(pdp_table[pdp_idx] & ~0xFFFULL);
	if (!(pd[pd_idx] & PRESENT)) return 0;
	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	if (!(pt[pt_idx] & PRESENT)) return 0;
	return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFF);
}
