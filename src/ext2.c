#include "ext2.h"
#include "ata.h"
#include "console.h"
#include "kstring.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define EXT2_SB_OFFSET 1024
#define EXT2_SB_MAGIC  0xEF53

#pragma pack(push, 1)
struct ext2_sb {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t r_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;
	uint32_t wtime;
	uint16_t mnt_count;
	uint16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev;
	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;
	uint16_t def_resuid;
	uint16_t def_resgid;
};

struct ext2_bg_desc {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad;
	uint32_t reserved[3];
};

struct ext2_inode {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t gid;
	uint16_t links_count;
	uint32_t blocks;
	uint32_t flags;
	uint32_t osd1;
	uint32_t block[15];
	uint32_t generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t faddr;
	uint32_t osd2[3];
};

struct ext2_dirent {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t  name_len;
	uint8_t  file_type;
	char     name[];
};
#pragma pack(pop)

static uint8_t g_drive;
static uint32_t g_part_start;
static uint32_t g_block_size;
static uint32_t g_inodes_per_group;
static struct ext2_sb g_sb;

static void read_inode(uint32_t inode_no, struct ext2_inode *inode);

static bool ext2_read_block_data(struct ext2_inode *inode, uint32_t logical_block, uint8_t *buf)
{
	uint32_t block_num = 0;
	if (logical_block < 12) {
		block_num = inode->block[logical_block];
	} else if (logical_block < 12 + 256) {
		uint32_t indirect_idx = logical_block - 12;
		uint32_t indirect_block = inode->block[12];
		if (!indirect_block) return false;
		uint32_t indirect_sector = g_part_start + (indirect_block * g_block_size) / 512;
		uint8_t tmp[512];
		uint32_t off = indirect_idx * sizeof(uint32_t);
		if (!ata_read_sectors(g_drive, indirect_sector + off / 512, 1, tmp))
			return false;
		kmemcpy(&block_num, tmp + (off % 512), sizeof(block_num));
	} else {
		return false;
	}
	if (!block_num) return false;
	uint32_t sector = g_part_start + (block_num * g_block_size) / 512;
	return ata_read_sectors(g_drive, sector, g_block_size / 512, buf);
}

static bool ext2_find_in_dir(uint32_t dir_inode, const char *name, uint8_t name_len, uint32_t *out_inode)
{
	struct ext2_inode inode;
	read_inode(dir_inode, &inode);
	uint8_t buf[4096];
	int max_blocks = 12 + 256;
	for (int blk = 0; blk < max_blocks; blk++) {
		uint32_t b = (blk < 12) ? inode.block[blk] : 0;
		if (blk >= 12) break;
		if (!b) continue;
		if (!ext2_read_block_data(&inode, blk, buf))
			break;
		uint32_t pos = 0;
		while (pos + 8 <= g_block_size) {
			struct ext2_dirent *de = (struct ext2_dirent *)(buf + pos);
			if (de->inode == 0 || de->rec_len == 0) break;
			if (de->name_len == name_len) {
				int match = 1;
				for (uint8_t c = 0; c < name_len; c++)
					if (name[c] != de->name[c]) { match = 0; break; }
				if (match) { *out_inode = de->inode; return true; }
			}
			pos += de->rec_len;
		}
	}
	return false;
}

static void read_inode(uint32_t inode_no, struct ext2_inode *inode)
{
	uint32_t group = (inode_no - 1) / g_inodes_per_group;
	uint32_t index = (inode_no - 1) % g_inodes_per_group;

	uint32_t bgd_block = g_block_size == 1024 ? 2 : 1;
	struct ext2_bg_desc bg;
	uint8_t tmp[512];
	uint32_t bgd_offset = group * sizeof(struct ext2_bg_desc);
	uint32_t bgd_sec = (bgd_block * g_block_size) / 512;
	ata_read_sectors(g_drive, g_part_start + bgd_sec + bgd_offset / 512, 1, tmp);
	kmemcpy(&bg, tmp + (bgd_offset % 512), sizeof(bg));

	uint32_t inode_table_block = bg.inode_table;
	uint32_t byte_offset = index * 128;
	uint32_t block = inode_table_block + (byte_offset / g_block_size);
	uint32_t offset = byte_offset % g_block_size;
	uint32_t sector = g_part_start + (block * g_block_size) / 512;

	ata_read_sectors(g_drive, sector, 1, tmp);
	kmemcpy(inode, tmp + offset, sizeof(struct ext2_inode));
}

static uint32_t find_inode(const char *path)
{
	uint32_t inode_no = 2;
	const char *part = path;

	while (part && *part) {
		while (*part == '/') part++;
		if (!*part) break;

		const char *next = part;
		while (*next && *next != '/') next++;

		uint32_t name_len = (uint8_t)(next - part);
		uint32_t found_ino = 0;
		if (!ext2_find_in_dir(inode_no, part, name_len, &found_ino))
			return 0;
		inode_no = found_ino;
		part = next;
	}
	return inode_no;
}

bool ext2_mount(uint8_t drive, uint32_t part_lba_start)
{
	g_drive = drive;
	g_part_start = part_lba_start;

	uint8_t tmp[512];
	uint32_t sb_sector = part_lba_start + (EXT2_SB_OFFSET / 512);
	if (!ata_read_sectors(drive, sb_sector, 1, tmp))
		return false;
	kmemcpy(&g_sb, tmp + (EXT2_SB_OFFSET % 512), sizeof(struct ext2_sb));

	if (g_sb.magic != EXT2_SB_MAGIC)
		return false;

	g_block_size = 1024 << g_sb.log_block_size;
	g_inodes_per_group = g_sb.inodes_per_group;

	char buf[16];
	console_print("[EXT2] Mounted: block_size=");
	kitoa(g_block_size, buf, sizeof(buf));
	console_print(buf);
	console_print(", inodes=");
	kitoa(g_sb.inodes_count, buf, sizeof(buf));
	console_print(buf);
	console_print("\n");
	return true;
}

bool ext2_read_file(const char *path, void *buf, uint32_t *size_out)
{
	uint32_t ino = find_inode(path);
	if (!ino) return false;

	struct ext2_inode inode;
	read_inode(ino, &inode);

	uint32_t size = inode.size;
	if (size_out) *size_out = size;

	uint32_t remaining = size;
	uint32_t offset = 0;
	int max_blocks = 12 + 256;

	for (int i = 0; i < max_blocks && remaining > 0; i++) {
		uint8_t tmp[4096];
		if (!ext2_read_block_data(&inode, i, tmp))
			break;

		uint32_t chunk = remaining > g_block_size ? g_block_size : remaining;
		kmemcpy((uint8_t *)buf + offset, tmp, chunk);
		offset += chunk;
		remaining -= chunk;
	}
	return true;
}

uint32_t ext2_get_file_size(const char *path)
{
	uint32_t ino = find_inode(path);
	if (!ino) return 0;

	struct ext2_inode inode;
	read_inode(ino, &inode);
	return inode.size;
}
