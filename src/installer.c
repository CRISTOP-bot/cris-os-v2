#include "installer.h"
#include "console.h"
#include "ata.h"
#include "part.h"
#include "ext2.h"
#include "asm.h"
#include "kstring.h"
#include "keyboard.h"
#include "fs.h"
#include "vfs.h"
#include "pmm.h"
#include <stdint.h>
#include <stdbool.h>

#define SECTOR_SIZE 512

struct mbr_entry {
	uint8_t  status;
	uint8_t  chs_start[3];
	uint8_t  type;
	uint8_t  chs_end[3];
	uint32_t lba_start;
	uint32_t sector_count;
} __attribute__((packed));

static void list_drives(void)
{
	console_print("Available ATA drives:\n");
	for (uint8_t d = 0; d < 4; d++) {
		if (!ata_is_present(d)) continue;
		console_print("  80");
		char c = '0' + d;
		console_print(&c);
		console_print(" — ");
		console_print(d & 1 ? "slave" : "master");
		console_print(" on ");
		console_print(d & 2 ? "secondary" : "primary");
		console_print("\n");
	}
}

static uint32_t get_drive_sectors(uint8_t drive)
{
	uint16_t io = (drive & 2) ? ATA_SECONDARY_IO : ATA_PRIMARY_IO;
	uint8_t drv = drive & 1;
	outb(io + 6, 0xA0 | (drv << 4));
	for (int t = 0; t < 10000000; t++)
		if (!(inb(io + 7) & 0x80)) break;
	outb(io + 7, 0xEC);
	for (int t = 0; t < 10000000; t++) {
		uint8_t st = inb(io + 7);
		if (st & 0x01) return 0;
		if (!(st & 0x80) && (st & 0x40)) break;
	}
	uint16_t ident[256];
	for (int i = 0; i < 256; i++) ident[i] = inw(io);
	if (ident[83] & (1 << 10))
		return *(uint32_t *)&ident[100];
	return *(uint32_t *)&ident[60];
}

static void prepare_mbr(uint8_t buf[SECTOR_SIZE], uint32_t part_lba, uint32_t part_sec)
{
	kmemset(buf, 0, SECTOR_SIZE);
	uint32_t last = part_lba + part_sec - 1;
	struct mbr_entry *p = (struct mbr_entry *)(buf + 446);
	p->status = 0x80;
	p->type = 0x83;
	p->lba_start = part_lba;
	p->sector_count = part_sec;
	p->chs_start[0] = 0;
	p->chs_start[1] = 1;
	p->chs_start[2] = 0;
	p->chs_end[0] = (last / (63 * 16)) & 0xFF;
	p->chs_end[1] = ((last % 63) + 1) | ((last / (63 * 16) >> 2) & 0xC0);
	p->chs_end[2] = ((last / 63) % 16) | ((last / (63 * 16) >> 8) & 0x80 ? 0x80 : 0);
	buf[510] = 0x55; buf[511] = 0xAA;
}

static int ext2_mkfs(uint8_t drive, uint32_t start_lba, uint32_t sectors)
{
	console_print("  Creating ext2...\n");
	uint32_t bs = 1024;
	uint32_t blocks = sectors / 2;
	uint32_t inodes = blocks / 4;
	if (inodes > 256) inodes = 256;
	uint32_t inode_tbl_blks = (inodes * 128 + bs - 1) / bs;
	uint32_t used = 5 + inode_tbl_blks;

	uint8_t sb[SECTOR_SIZE];
	kmemset(sb, 0, SECTOR_SIZE);
	struct {
		uint32_t inodes_count, blocks_count, r_blocks_count, free_blocks_count;
		uint32_t free_inodes_count, first_data_block, log_block_size, log_frag_size;
		uint32_t blocks_per_group, frags_per_group, inodes_per_group;
		uint32_t mtime, wtime;
		uint16_t mnt_count, max_mnt_count, magic, state, errors, minor_rev;
		uint32_t lastcheck, checkinterval, creator_os, rev_level;
		uint16_t def_resuid, def_resgid;
	} __attribute__((packed)) *sbp = (void *)(sb + (1024 % 512));

	sbp->inodes_count = inodes;
	sbp->blocks_count = blocks;
	sbp->free_blocks_count = blocks - used;
	sbp->free_inodes_count = inodes - 2;
	sbp->first_data_block = 1;
	sbp->log_block_size = 0;
	sbp->blocks_per_group = blocks;
	sbp->inodes_per_group = inodes;
	sbp->magic = 0xEF53;
	sbp->state = 1;
	sbp->errors = 1;

	if (!ata_write_sectors(drive, start_lba + 2, 1, sb)) return -1;

	/* Block group descriptor */
	uint8_t bgd[512];
	kmemset(bgd, 0, 512);
	struct { uint32_t bmap, imap, itable; uint16_t free_b, free_i, used_dirs; uint16_t pad; uint32_t res[3]; } *bg = (void *)bgd;
	bg->bmap = 3;
	bg->imap = 4;
	bg->itable = 5;
	bg->free_b = sbp->free_blocks_count;
	bg->free_i = sbp->free_inodes_count;
	bg->used_dirs = 1;

	if (!ata_write_sectors(drive, start_lba + (1 * bs) / 512, 1, bgd)) return -1;

	/* Block bitmap */
	uint8_t bmap[1024];
	kmemset(bmap, 0, 1024);
	for (uint32_t i = 0; i < used; i++) bmap[i / 8] |= (1 << (i % 8));
	if (!ata_write_sectors(drive, start_lba + (3 * bs) / 512, 2, bmap)) return -1;

	/* Inode bitmap */
	uint8_t imap[512];
	kmemset(imap, 0, 512);
	imap[0] = 0x03;
	if (!ata_write_sectors(drive, start_lba + (4 * bs) / 512, 1, imap)) return -1;

	/* Inode table */
	uint8_t itbl[2048];
	kmemset(itbl, 0, 2048);
	struct {
		uint16_t mode, uid; uint32_t size; uint32_t atime, ctime, mtime, dtime;
		uint16_t gid, links; uint32_t blocks, flags, osd1;
		uint32_t block[15];
	} __attribute__((packed)) *in = (void *)(itbl + (2 - 1) * 128);
	in->mode = 0x41ED;
	in->size = bs;
	in->links = 2;
	in->block[0] = used;

	if (!ata_write_sectors(drive, start_lba + (5 * bs) / 512, 4, itbl)) return -1;

	/* Root directory data */
	uint8_t root_data[1024];
	kmemset(root_data, 0, 1024);
	uint32_t pos = 0;
	struct { uint32_t ino; uint16_t rec; uint8_t nlen, ftype; char name[1]; } *de;
	de = (void *)(root_data + pos); de->ino = 2; de->rec = 12; de->nlen = 1; de->ftype = 2; de->name[0] = '.'; pos += 12;
	de = (void *)(root_data + pos); de->ino = 2; de->rec = bs - 12; de->nlen = 2; de->ftype = 2; de->name[0] = '.'; de->name[1] = '.';

	if (!ata_write_sectors(drive, start_lba + (used * bs) / 512, bs / 512, root_data)) return -1;

	return (int)used + 1;
}

static int ext2_mkfile(uint8_t drive, uint32_t start_lba, uint32_t next_block,
                       const char *name, const void *data, uint32_t size)
{
	uint32_t bs = 1024;
	uint8_t imap[512];
	if (!ata_read_sectors(drive, start_lba + (4 * bs) / 512, 1, imap)) return -1;

	uint32_t ino = 2;
	while (ino < 256 && (imap[ino / 8] & (1 << (ino % 8)))) ino++;
	if (ino >= 256) return -1;
	imap[ino / 8] |= (1 << (ino % 8));
	if (!ata_write_sectors(drive, start_lba + (4 * bs) / 512, 1, imap)) return -1;

	uint8_t itbl[2048];
	if (!ata_read_sectors(drive, start_lba + (5 * bs) / 512, 4, itbl)) return -1;

	struct {
		uint16_t mode, uid; uint32_t size; uint32_t pad[4];
		uint16_t gid, links; uint32_t blocks, flags;
		uint32_t block[15];
	} __attribute__((packed)) *in = (void *)(itbl + (ino - 1) * 128);
	in->mode = 0x81A4;
	in->size = size;
	in->links = 1;

	uint32_t remaining = size;
	uint32_t offset = 0;
	int bi = 0;
	while (remaining > 0 && bi < 12) {
		in->block[bi] = next_block;
		uint8_t buf[1024];
		kmemset(buf, 0, 1024);
		uint32_t chunk = remaining > bs ? bs : remaining;
		kmemcpy(buf, (const uint8_t *)data + offset, chunk);

		if (!ata_write_sectors(drive, start_lba + (next_block * bs) / 512, bs / 512, buf))
			return -1;
		next_block++;
		offset += chunk;
		remaining -= chunk;
		bi++;
	}

	if (!ata_write_sectors(drive, start_lba + (5 * bs) / 512, 4, itbl)) return -1;

	/* Add directory entry in root (inode 2) */
	uint32_t root_data_blk = 0;
	{
		uint8_t tmp[2048];
		ata_read_sectors(drive, start_lba + (5 * bs) / 512, 4, tmp);
		root_data_blk = ((struct { uint32_t block[15]; } *)(tmp + (2 - 1) * 128))->block[0];
	}

	uint8_t dir[1024];
	if (!ata_read_sectors(drive, start_lba + (root_data_blk * bs) / 512, bs / 512, dir))
		return -1;

	uint32_t nlen = kstrlen(name);
	uint32_t reclen = 8 + nlen + ((4 - (nlen % 4)) % 4);
	if (reclen < 12) reclen = 12;

	uint32_t dp = 0;
	while (dp < bs) {
		struct { uint32_t ino; uint16_t rec; uint8_t nlen, ftype; char name[64]; } *e = (void *)(dir + dp);
		if (e->ino == 0 || dp + e->rec >= bs) {
			e->ino = ino;
			e->rec = reclen;
			e->nlen = nlen;
			e->ftype = 1;
			kmemcpy(e->name, name, nlen);
			break;
		}
		dp += e->rec;
	}

	if (!ata_write_sectors(drive, start_lba + (root_data_blk * bs) / 512, bs / 512, dir))
		return -1;

	return (int)next_block;
}

void handle_install_command(const char *args)
{
	console_print_color("\n======== NucleOS Installer ========\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));

	uint8_t drive = 0;
	if (args && args[0]) {
		drive = (uint8_t)katoi(args);
		if (drive > 3) {
			console_print("Usage: install [0-3]\n  0=primary master, 1=primary slave\n  2=secondary master, 3=secondary slave\n");
			list_drives();
			return;
		}
	} else {
		list_drives();
		console_print("Select drive (0-3): ");
		char buf[8];
		if (!keyboard_readline(buf, sizeof(buf))) return;
		drive = (uint8_t)katoi(buf);
		if (drive > 3) { console_print_color("Invalid\n", VGA_ATTR(VGA_RED, VGA_BLACK)); return; }
	}

	if (!ata_is_present(drive)) {
		console_print_color("Drive not present\n", VGA_ATTR(VGA_RED, VGA_BLACK));
		return;
	}

	uint32_t sectors = get_drive_sectors(drive);
	if (!sectors) {
		console_print_color("Cannot detect drive size\n", VGA_ATTR(VGA_RED, VGA_BLACK));
		return;
	}

	char tmp[16];
	kitoa(sectors / 2048, tmp, sizeof(tmp));
	console_print("Drive: 80"); tmp[0] = '0' + (char)drive; tmp[1] = '\0'; console_print(tmp);
	console_print("  Size: "); console_print(tmp); console_print(" MB\n");

	console_print_color("WARNING: All data will be erased!\n", VGA_ATTR(VGA_RED, VGA_BLACK));
	console_print("Continue? (y/n): ");
	char ans[4];
	if (!keyboard_readline(ans, 4) || (ans[0] != 'y' && ans[0] != 'Y')) {
		console_print("Cancelled.\n"); return;
	}

	uint32_t part_lba = 2048;
	uint32_t part_sec = sectors - part_lba;

	uint8_t mbr[512];
	prepare_mbr(mbr, part_lba, part_sec);
	if (!ata_write_sectors(drive, 0, 1, mbr)) {
		console_print_color("MBR write failed\n", VGA_ATTR(VGA_RED, VGA_BLACK));
		return;
	}
	console_print("  MBR written\n");

	int next = ext2_mkfs(drive, part_lba, part_sec);
	if (next < 0) {
		console_print_color("FS creation failed\n", VGA_ATTR(VGA_RED, VGA_BLACK));
		return;
	}
	console_print("  ext2 filesystem created\n");

	/* Try to get kernel.bin and rootfs.bin from VFS */
	const char *sources[] = {"/boot/kernel.bin", "/boot/rootfs.bin"};
	for (int i = 0; i < 2; i++) {
		const char *spath = sources[i];
		const void *data = vfs_read(spath);
		uint32_t sz = vfs_get_size(spath);
		if (!data || !sz) {
			/* Fallback: try without leading / */
			const char *alt = spath + 1;
			data = vfs_read(alt);
			sz = vfs_get_size(alt);
		}
		if (!data || !sz) {
			/* Fallback: rootfs_find */
			const struct fs_file *f = fs_find(spath);
			if (f) { data = f->data; sz = f->size; }
		}
		if (!data || !sz) {
			console_print_color("  ", VGA_ATTR(VGA_RED, VGA_BLACK));
			console_print(spath);
			console_print(" not found, skipping\n");
			continue;
		}

		const char *fname = spath;
		while (*fname) fname++;
		while (fname > spath && fname[-1] != '/') fname--;

		console_print("  Writing ");
		console_print(fname);
		console_print("...\n");

		next = ext2_mkfile(drive, part_lba, (uint32_t)next, fname, data, sz);
		if (next < 0) {
			console_print_color("  Write failed\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			return;
		}
	}

	console_print_color("\nInstallation complete!\n", VGA_ATTR(VGA_GREEN, VGA_BLACK));
	console_print("Files written. To boot:\n");
	console_print("  Install GRUB from a Linux Live USB:\n");
	console_print("    grub-install --target=i386-pc /dev/sdX\n");
	console_print("    grub-mkconfig -o /boot/grub/grub.cfg\n");
	console_print("  Or manually create a Multiboot entry.\n");
}
