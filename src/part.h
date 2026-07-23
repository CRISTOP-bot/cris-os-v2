#ifndef PART_H
#define PART_H

#include <stdint.h>
#include <stdbool.h>

#define PART_TYPE_LINUX  0x83
#define PART_TYPE_FAT32  0x0C
#define PART_TYPE_EFI    0xEE

struct part_entry {
	uint8_t  status;
	uint8_t  chs_start[3];
	uint8_t  type;
	uint8_t  chs_end[3];
	uint32_t lba_start;
	uint32_t sector_count;
};

struct mbr {
	uint8_t  bootcode[446];
	struct part_entry partitions[4];
	uint16_t signature;
};

bool part_read_mbr(uint8_t drive, struct mbr *mbr);
int part_find_by_type(uint8_t drive, uint8_t type, struct part_entry *out);
bool part_read_sectors(uint8_t drive, const struct part_entry *part,
                       uint32_t lba, uint8_t count, void *buf);

#endif
