#include "ata.h"
#include "asm.h"
#include "console.h"
#include "kstring.h"
#include "timer.h"
#include <stdint.h>
#include <stdbool.h>

#define DATA_PORT(io)   (io)
#define ERROR_PORT(io)  (io + 1)
#define SECCOUNT_PORT(io) (io + 2)
#define LBA_LO_PORT(io) (io + 3)
#define LBA_MID_PORT(io) (io + 4)
#define LBA_HI_PORT(io) (io + 5)
#define DRIVE_PORT(io)  (io + 6)
#define STATUS_PORT(io) (io + 7)
#define CMD_PORT(io)    (io + 7)

#define CMD_READ_PIO     0x20
#define CMD_READ_PIO_EXT 0x24
#define CMD_WRITE_PIO    0x30
#define CMD_IDENTIFY     0xEC
#define CMD_FLUSH_CACHE  0xE7

#define STATUS_ERR  0x01
#define STATUS_DRQ  0x08
#define STATUS_SRV  0x10
#define STATUS_DF   0x20
#define STATUS_RDY  0x40
#define STATUS_BSY  0x80

#define MAX_RETRIES 3
#define TIMEOUT_IN_TICKS 5

static uint16_t ata_io_base(uint8_t drive)
{
	return (drive & 2) ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
}

static uint16_t ata_ctrl_base(uint8_t drive)
{
	return (drive & 2) ? ATA_SECONDARY_CTRL : ATA_PRIMARY_CTRL;
}

static int wait_not_bsy(uint16_t io)
{
	unsigned long timeout = timer_get_ticks() + TIMEOUT_IN_TICKS;
	while (inb(STATUS_PORT(io)) & STATUS_BSY) {
		if (timer_get_ticks() >= timeout)
			return -1;
	}
	return 0;
}

static int wait_drq(uint16_t io)
{
	unsigned long timeout = timer_get_ticks() + TIMEOUT_IN_TICKS;
	while (1) {
		uint8_t st = inb(STATUS_PORT(io));
		if (st & (STATUS_ERR | STATUS_DF))
			return -1;
		if (!(st & STATUS_BSY) && (st & STATUS_DRQ))
			return 0;
		if (timer_get_ticks() >= timeout)
			return -1;
	}
}

static int ata_poll(uint16_t io)
{
	if (wait_not_bsy(io) < 0)
		return -1;
	uint8_t st = inb(STATUS_PORT(io));
	if (st & (STATUS_ERR | STATUS_DF))
		return -1;
	return 0;
}

static bool ata_identify(uint8_t drive, uint16_t io)
{
	outb(ata_ctrl_base(drive), 0x04);
	outb(ata_ctrl_base(drive), 0x00);

	outb(DRIVE_PORT(io), 0xA0 | ((drive & 1) << 4));
	if (wait_not_bsy(io) < 0) return false;

	outb(CMD_PORT(io), CMD_IDENTIFY);
	if (wait_not_bsy(io) < 0) return false;

	uint8_t st = inb(STATUS_PORT(io));
	if (!st) return false;

	if (inb(LBA_MID_PORT(io)) == 0xEB && inb(LBA_HI_PORT(io)) == 0x14)
		return false; /* ATAPI */
	if (inb(LBA_MID_PORT(io)) == 0xFF && inb(LBA_HI_PORT(io)) == 0xFF)
		return false;

	return true;
}

bool ata_init(void)
{
	bool found = false;
	int drives_found = 0;

	for (uint8_t drive = 0; drive < 4; drive++) {
		uint16_t io = ata_io_base(drive);
		if (ata_identify(drive, io)) {
			drives_found++;
			found = true;
		}
	}

	char buf[16];
	console_print("[ATA] ");
	kitoa(drives_found, buf, sizeof(buf));
	console_print(buf);
	console_print(" drive(s) found\n");
	return found;
}

bool ata_is_present(uint8_t drive)
{
	if (drive > 3) return false;
	return ata_identify(drive, ata_io_base(drive));
}

bool ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buf)
{
	uint16_t io = ata_io_base(drive);
	uint8_t drv = 0xE0 | ((drive & 1) << 4);

	for (int retry = 0; retry < MAX_RETRIES; retry++) {
		if (retry > 0) {
			outb(ata_ctrl_base(drive), 0x04);
			outb(ata_ctrl_base(drive), 0x00);
		}

		if (wait_not_bsy(io) < 0) continue;

		outb(DRIVE_PORT(io), drv | ((lba >> 24) & 0x0F));
		outb(SECCOUNT_PORT(io), count);
		outb(LBA_LO_PORT(io), (uint8_t)(lba));
		outb(LBA_MID_PORT(io), (uint8_t)(lba >> 8));
		outb(LBA_HI_PORT(io), (uint8_t)(lba >> 16));
		outb(CMD_PORT(io), CMD_READ_PIO);

		if (ata_poll(io) < 0) continue;

		uint16_t *ptr = (uint16_t *)buf;
		int ok = 1;
		for (int s = 0; s < count; s++) {
			if (ata_poll(io) < 0) { ok = 0; break; }
			for (int i = 0; i < 256; i++)
				ptr[i] = inw(io);
			ptr += 256;
		}
		if (ok) return true;
	}
	return false;
}

bool ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buf)
{
	uint16_t io = ata_io_base(drive);
	uint8_t drv = 0xE0 | ((drive & 1) << 4);

	for (int retry = 0; retry < MAX_RETRIES; retry++) {
		if (retry > 0) {
			outb(ata_ctrl_base(drive), 0x04);
			outb(ata_ctrl_base(drive), 0x00);
		}

		if (wait_not_bsy(io) < 0) continue;

		outb(DRIVE_PORT(io), drv | ((lba >> 24) & 0x0F));
		outb(SECCOUNT_PORT(io), count);
		outb(LBA_LO_PORT(io), (uint8_t)(lba));
		outb(LBA_MID_PORT(io), (uint8_t)(lba >> 8));
		outb(LBA_HI_PORT(io), (uint8_t)(lba >> 16));
		outb(CMD_PORT(io), CMD_WRITE_PIO);

		if (wait_not_bsy(io) < 0) continue;

		const uint16_t *ptr = (const uint16_t *)buf;
		int ok = 1;
		for (int s = 0; s < count; s++) {
			if (wait_drq(io) < 0) { ok = 0; break; }
			for (int i = 0; i < 256; i++)
				outw(io, ptr[i]);
			ptr += 256;
		}
		if (ok) {
			outb(CMD_PORT(io), CMD_FLUSH_CACHE);
			wait_not_bsy(io);
			return true;
		}
	}
	return false;
}
