#!/usr/bin/env python3
import os
import struct
import sys

MAGIC = b'CRFS'
VERSION = 1
HEADER_FMT = '<4sIII'
ENTRY_FMT = '<16sII'
HEADER_SIZE = struct.calcsize(HEADER_FMT)
ENTRY_SIZE = struct.calcsize(ENTRY_FMT)


def pad_name(name):
    data = name.encode('utf-8')
    if len(data) > 15:
        raise ValueError('File name too long: %s' % name)
    return data + b'\x00' * (16 - len(data))


def build_rootfs(rootdir, output_path):
    files = []
    for filename in sorted(os.listdir(rootdir)):
        path = os.path.join(rootdir, filename)
        if os.path.isfile(path):
            with open(path, 'rb') as f:
                data = f.read()
            files.append((filename, data))

    offset = HEADER_SIZE + len(files) * ENTRY_SIZE
    offset = (offset + 3) & ~3
    entries = []
    contents = b''
    for name, data in files:
        entries.append((pad_name(name), offset, len(data)))
        contents += data
        pad = (-len(data)) & 3
        if pad:
            contents += b'\x00' * pad
        offset += len(data) + pad

    with open(output_path, 'wb') as out:
        out.write(struct.pack(HEADER_FMT, MAGIC, VERSION, len(files), 0))
        for name, off, size in entries:
            out.write(struct.pack(ENTRY_FMT, name, off, size))
        current = out.tell()
        pad = (-current) & 3
        if pad:
            out.write(b'\x00' * pad)
        out.write(contents)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: build_rootfs.py <rootfs_dir> <output_path>')
        sys.exit(1)
    build_rootfs(sys.argv[1], sys.argv[2])
