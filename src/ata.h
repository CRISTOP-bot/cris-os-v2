#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_IO   0x170
#define ATA_SECONDARY_CTRL 0x376

bool ata_init(void);
bool ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buf);
bool ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buf);
bool ata_is_present(uint8_t drive);

#endif
