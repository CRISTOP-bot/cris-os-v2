#!/usr/bin/env python3
"""Minimal ISO 9660 + El Torito No-Emulation boot for CrisOS."""

import struct, os, sys

SECTOR = 2048

def pad(data, n):
    return data + b'\0' * (n - len(data))

def make_iso(kernel_path, output_path):
    with open(kernel_path, 'rb') as f:
        kernel = f.read()
    kernel_sectors = (len(kernel) + SECTOR - 1) // SECTOR
    kernel_padded = pad(kernel, kernel_sectors * SECTOR)

    # --- Boot catalog (El Torito) at LBA 17 ---
    boot_catalog = b'\x01'                               # Validation entry
    boot_catalog += b'\x00'                              # Platform (x86)
    boot_catalog += b'\x00' * 4                          # reserved
    boot_catalog += struct.pack('<H', 0xAA55)            # key
    boot_catalog += b'\x55\xAA'                          # key2
    boot_catalog += b'\x00' * 20                         # reserved
    boot_catalog += b'\x88'                              # Initial/Default entry, bootable
    boot_catalog += b'\x00'                              # media type (no emulation)
    boot_catalog += b'\x00\x00'                          # load segment
    boot_catalog += b'\x00'                              # system type
    boot_catalog += b'\x00'                              # reserved
    boot_catalog += struct.pack('<H', kernel_sectors)     # sector count
    boot_catalog += struct.pack('<I', 20)                 # LBA of kernel
    boot_catalog += b'\x00' * 20
    boot_catalog = pad(boot_catalog, SECTOR)

    # --- Volume Descriptors ---
    def make_vol_desc(typ, root_lba, root_size, path_lba):
        vd = bytes([typ])                                # type
        vd += b'CD001\x01\x00'                          # identifier + version
        vd += b'\x00' * 8                                # system
        vd += pad(b'CRISOS', 32)                         # volume id
        vd += b'\x00' * 8
        total_sectors = 35 + kernel_sectors
        vd += struct.pack('<I', total_sectors)            # vol space size (LE)
        vd += struct.pack('>I', total_sectors)            # vol space size (BE)
        vd += b'\x00' * 32
        vd += struct.pack('<H', 1) + b'\x00' * 2
        vd += struct.pack('<H', 1) + b'\x00' * 2
        vd += struct.pack('<H', SECTOR) + b'\x00' * 2
        vd += struct.pack('<I', root_size) * 2           # path table size
        vd += struct.pack('<I', path_lba)                # L path table LB
        vd += b'\x00' * 4
        vd += struct.pack('>I', path_lba)                # M path table LB
        vd += b'\x00' * 4
        # Root directory record (34 bytes)
        root_rec = struct.pack('<B', 34)                  # length
        root_rec += b'\x00'                               # xattr length
        root_rec += struct.pack('<I', root_lba)           # ext loc (LE)
        root_rec += struct.pack('>I', root_lba)           # ext loc (BE)
        root_rec += struct.pack('<I', root_size)          # data len (LE)
        root_rec += struct.pack('>I', root_size)          # data len (BE)
        root_rec += b'\x00' * 7 + b'\x02' + b'\x00' * 3
        root_rec += struct.pack('<H', 1) + b'\x00' * 2
        root_rec += b'\x01' + b'\x00'
        vd += root_rec
        vd += b'\x00' * 7 + b'\x00' + b'\x00' * 6
        vd += b'\x00' * 367
        return pad(vd, SECTOR)

    # Layout:
    # LBA 0:  Primary Volume Descriptor
    # LBA 1:  Terminator Volume Descriptor
    # LBA 2-16: padding
    # LBA 17: Boot Catalog
    # LBA 18: Root Directory
    # LBA 19: Path Table
    # LBA 20: Kernel data

    root_lba = 18
    root_size = SECTOR
    path_lba = 19
    primary = make_vol_desc(1, root_lba, root_size, path_lba)
    terminator = make_vol_desc(255, root_lba, root_size, path_lba)

    with open(output_path, 'wb') as f:
        f.write(primary)          # LBA 0
        f.write(terminator)       # LBA 1
        f.write(b'\0' * SECTOR * 16)  # LBA 2-17: padding for boot_catalog
        f.seek(17 * SECTOR)
        f.write(boot_catalog)     # LBA 17
        # Root directory at LBA 18
        root_dir = b''
        # "." entry
        dot = struct.pack('<B', 34)
        dot += b'\x00' + struct.pack('<II', root_lba, root_lba)
        dot += struct.pack('<II', root_size, root_size)
        dot += b'\x00' * 7 + b'\x02' + b'\x00' * 3
        dot += struct.pack('<H', 1) + b'\x00' * 2
        dot += b'\x01' + b'\x00'
        root_dir += dot
        # ".." entry
        dotdot = struct.pack('<B', 34)
        dotdot += b'\x00' + struct.pack('<II', root_lba, root_lba)
        dotdot += struct.pack('<II', root_size, root_size)
        dotdot += b'\x00' * 7 + b'\x02' + b'\x00' * 3
        dotdot += struct.pack('<H', 1) + b'\x00' * 2
        dotdot += b'\x01' + b'\x00'
        root_dir += dotdot
        # KERNEL.BIN entry
        kname = b'KERNEL.BIN'
        kentry = struct.pack('<B', 33 + len(kname))
        kentry += b'\x00'
        kentry += struct.pack('<II', 20, 20)  # LBA of kernel
        kentry += struct.pack('<II', len(kernel), len(kernel))
        kentry += b'\x00' * 7 + b'\x00' + b'\x00' * 3
        kentry += struct.pack('<H', 1) + b'\x00' * 2
        kentry += bytes([len(kname)]) + kname
        root_dir += kentry
        root_dir = pad(root_dir, SECTOR)
        f.seek(18 * SECTOR)
        f.write(root_dir)
        # Path table at LBA 19
        path = struct.pack('<B', 1)
        path += b'\x00'
        path += struct.pack('<II', root_lba, root_lba)
        path += struct.pack('<H', 1) + b'\x00' * 2
        path = pad(path, SECTOR)
        f.seek(19 * SECTOR)
        f.write(path)
        # Kernel at LBA 20
        f.seek(20 * SECTOR)
        f.write(kernel_padded)

    print(f"ISO created: {output_path} ({os.path.getsize(output_path)} bytes)")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <kernel.bin> <output.iso>")
        sys.exit(1)
    make_iso(sys.argv[1], sys.argv[2])
