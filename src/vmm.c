#include "vmm.h"
#include "pmm.h"
#include "kstring.h"
#include "console.h"
#include <stdint.h>

#define PRESENT  (1ULL << 0)
#define WRITE    (1ULL << 1)
#define USER     (1ULL << 2)
#define PS       (1ULL << 7)
#define NX       (1ULL << 63)

static inline uint64_t *get_pml4(void)
{
	uint64_t cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	return (uint64_t *)(cr3 & ~0xFFFULL);
}

static inline void invlpg(unsigned long addr)
{
	__asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void vmm_init(unsigned long mem_lower, unsigned long mem_upper)
{
	(void)mem_lower;
	unsigned long total_ram = 1024 + mem_upper;
	unsigned long total_bytes = total_ram * 1024;

	uint64_t *pml4 = get_pml4();
	if (!(pml4[0] & PRESENT)) {
		console_print("[VMM] ERROR: PML4[0] not present\n");
		return;
	}
	uint64_t *pdpt = (uint64_t *)(pml4[0] & ~0xFFFULL);
	if (!(pdpt[0] & PRESENT)) {
		console_print("[VMM] ERROR: PDPT[0] not present\n");
		return;
	}
	uint64_t *pd = (uint64_t *)(pdpt[0] & ~0xFFFULL);

	unsigned long num_2mb = (total_bytes + 0x1FFFFF) / 0x200000;
	if (num_2mb > 512)
		num_2mb = 512;

	for (unsigned long i = 8; i < num_2mb; i++) {
		if (!(pd[i] & PRESENT))
			pd[i] = (i * 0x200000) | 0x83;
	}

	char buf[32];
	console_print("[VMM] Extended identity map to ");
	kitoa((long)(num_2mb * 2), buf, sizeof(buf));
	console_print(buf);
	console_print(" MB\n");
}

void vmm_map_page(unsigned long phys, unsigned long virt, unsigned int flags)
{
	unsigned int pml4_idx = (virt >> 39) & 0x1FF;
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	uint64_t *pml4 = get_pml4();
	uint64_t entry_flags = (uint64_t)(flags & 0x1F) | PRESENT;

	if (!(pml4[pml4_idx] & PRESENT)) {
		uint64_t *new_pdp = (uint64_t *)pmm_alloc_page();
		if (!new_pdp) return;
		kmemset(new_pdp, 0, 4096);
		pml4[pml4_idx] = (uint64_t)(unsigned long)new_pdp | entry_flags;
	}
	uint64_t *pdp = (uint64_t *)(pml4[pml4_idx] & ~0xFFFULL);

	if (!(pdp[pdp_idx] & PRESENT)) {
		uint64_t *new_pd = (uint64_t *)pmm_alloc_page();
		if (!new_pd) return;
		kmemset(new_pd, 0, 4096);
		pdp[pdp_idx] = (uint64_t)(unsigned long)new_pd | entry_flags;
	}
	uint64_t *pd = (uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);

	if (pd[pd_idx] & PS) {
		uint64_t base_phys = pd[pd_idx] & ~0x3FFFFFULL;
		uint64_t base_flags = pd[pd_idx] & 0x1FF;
		uint64_t *new_pt = (uint64_t *)pmm_alloc_page();
		if (!new_pt) return;
		for (int i = 0; i < 512; i++)
			new_pt[i] = (base_phys + i * 4096) | base_flags;
		pd[pd_idx] = (uint64_t)(unsigned long)new_pt | (base_flags & ~PS);
	}

	if (!(pd[pd_idx] & PRESENT)) {
		uint64_t *new_pt = (uint64_t *)pmm_alloc_page();
		if (!new_pt) return;
		kmemset(new_pt, 0, 4096);
		pd[pd_idx] = (uint64_t)(unsigned long)new_pt | entry_flags;
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

	uint64_t *pml4 = get_pml4();
	if (!(pml4[0] & PRESENT)) return;
	uint64_t *pdp = (uint64_t *)(pml4[0] & ~0xFFFULL);
	if (!(pdp[pdp_idx] & PRESENT)) return;
	uint64_t *pd = (uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);
	if (pd[pd_idx] & PS) return;
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

	uint64_t *pml4 = get_pml4();
	if (!(pml4[0] & PRESENT)) return false;
	uint64_t *pdp = (uint64_t *)(pml4[0] & ~0xFFFULL);
	if (!(pdp[pdp_idx] & PRESENT)) return false;
	uint64_t *pd = (uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);
	if (pd[pd_idx] & PS) return true;
	if (!(pd[pd_idx] & PRESENT)) return false;
	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	return (pt[pt_idx] & PRESENT) != 0;
}

unsigned long vmm_get_phys(unsigned long virt)
{
	unsigned int pdp_idx  = (virt >> 30) & 0x1FF;
	unsigned int pd_idx   = (virt >> 21) & 0x1FF;
	unsigned int pt_idx   = (virt >> 12) & 0x1FF;

	uint64_t *pml4 = get_pml4();
	if (!(pml4[0] & PRESENT)) return 0;
	uint64_t *pdp = (uint64_t *)(pml4[0] & ~0xFFFULL);
	if (!(pdp[pdp_idx] & PRESENT)) return 0;
	uint64_t *pd = (uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);
	if (pd[pd_idx] & PS)
		return (pd[pd_idx] & ~0x3FFFFFULL) | (virt & 0x3FFFFF);
	if (!(pd[pd_idx] & PRESENT)) return 0;
	uint64_t *pt = (uint64_t *)(pd[pd_idx] & ~0xFFFULL);
	if (!(pt[pt_idx] & PRESENT)) return 0;
	return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFF);
}
