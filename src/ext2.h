#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool ext2_mount(uint8_t drive, uint32_t part_lba_start);
bool ext2_read_file(const char *path, void *buf, uint32_t *size_out);
uint32_t ext2_get_file_size(const char *path);

#endif
