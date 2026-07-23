#include "part.h"
#include "ata.h"
#include "console.h"
#include "kstring.h"
#include <stdint.h>
#include <stdbool.h>

bool part_read_mbr(uint8_t drive, struct mbr *mbr)
{
	if (!ata_read_sectors(drive, 0, 1, mbr))
		return false;
	if (mbr->signature != 0xAA55)
		return false;
	return true;
}

int part_find_by_type(uint8_t drive, uint8_t type, struct part_entry *out)
{
	struct mbr mbr;
	if (!part_read_mbr(drive, &mbr))
		return -1;

	int found = 0;
	for (int i = 0; i < 4; i++) {
		if (mbr.partitions[i].type == type) {
			*out = mbr.partitions[i];
			found++;
		}
	}
	return found;
}

bool part_read_sectors(uint8_t drive, const struct part_entry *part,
                       uint32_t lba, uint8_t count, void *buf)
{
	return ata_read_sectors(drive, part->lba_start + lba, count, buf);
}
