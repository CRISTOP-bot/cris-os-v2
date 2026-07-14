#ifndef VMM_H
#define VMM_H

#include <stdbool.h>

void vmm_init(unsigned long mem_lower, unsigned long mem_upper);
void vmm_map_page(unsigned long phys, unsigned long virt, unsigned int flags);
void vmm_unmap_page(unsigned long virt);
bool vmm_is_page_mapped(unsigned long virt);
unsigned long vmm_get_phys(unsigned long virt);

#define VMM_PRESENT  0x01
#define VMM_WRITE    0x02
#define VMM_USER     0x04
#define VMM_WRITE_THROUGH 0x08
#define VMM_CACHE_DISABLE 0x10

#endif /* VMM_H */
