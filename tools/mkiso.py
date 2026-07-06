#!/usr/bin/env python3
"""
mkiso.py — Build a GRUB Multiboot ISO for CrisOS v3.

What it does:
- Copies the kernel into iso/boot/kernel.bin
- Packs rootfs/ into iso/boot/rootfs.cpio (newc cpio archive)
- Writes a GRUB config that boots the kernel + module
- Builds the final ISO using grub-mkrescue

Requirements:
- grub-mkrescue (or grub2-mkrescue on some distros)
- xorriso
- QEMU (only if you use --run)

Recommended project layout:
.
├── build/
│   ├── kernel.bin
│   └── ...
├── rootfs/
│   └── ...
├── tools/
│   └── mkiso.py
└── boot/ (optional, not required by this script)
"""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable, Iterator, Tuple


# ----------------------------
# Helpers
# ----------------------------

def die(message: str, code: int = 1) -> "None":
    print(f"[mkiso] error: {message}", file=sys.stderr)
    raise SystemExit(code)


def info(message: str) -> None:
    print(f"[mkiso] {message}")


def which_or_none(*names: str) -> str | None:
    for name in names:
        path = shutil.which(name)
        if path:
            return path
    return None


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def rm_tree(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink(missing_ok=True)
    elif path.is_dir():
        shutil.rmtree(path, ignore_errors=True)


def copy_file(src: Path, dst: Path) -> None:
    ensure_parent(dst)
    shutil.copy2(src, dst)


def write_text(path: Path, text: str) -> None:
    ensure_parent(path)
    path.write_text(text, encoding="utf-8", newline="\n")


def pad4(n: int) -> int:
    return (-n) % 4


# ----------------------------
# CPIO newc builder
# ----------------------------

class NewcBuilder:
    """
    Minimal 'newc' cpio archive builder.

    Supports:
    - regular files
    - directories
    - symlinks

    This is enough for a rootfs module used by a small OS kernel.
    """

    def __init__(self) -> None:
        self._ino = 1
        self._chunks: list[bytes] = []

    def _hex8(self, n: int) -> bytes:
        return f"{n & 0xFFFFFFFF:08x}".encode("ascii")

    def _add_entry(
        self,
        name: str,
        mode: int,
        data: bytes = b"",
        mtime: int | None = None,
        nlink: int = 1,
        uid: int = 0,
        gid: int = 0,
        devmajor: int = 0,
        devminor: int = 0,
        rdevmajor: int = 0,
        rdevminor: int = 0,
    ) -> None:
        if mtime is None:
            mtime = int(time.time())

        name_b = name.encode("utf-8")
        namesize = len(name_b) + 1
        filesize = len(data)

        header = b"".join([
            b"070701",                  # c_magic
            self._hex8(self._ino),      # c_ino
            self._hex8(mode),           # c_mode
            self._hex8(uid),            # c_uid
            self._hex8(gid),            # c_gid
            self._hex8(nlink),          # c_nlink
            self._hex8(mtime),           # c_mtime
            self._hex8(filesize),       # c_filesize
            self._hex8(devmajor),       # c_devmajor
            self._hex8(devminor),       # c_devminor
            self._hex8(rdevmajor),      # c_rdevmajor
            self._hex8(rdevminor),      # c_rdevminor
            self._hex8(namesize),       # c_namesize
            self._hex8(0),              # c_check
        ])

        self._chunks.append(header)
        self._chunks.append(name_b + b"\x00")
        if pad4(namesize):
            self._chunks.append(b"\x00" * pad4(namesize))

        if filesize:
            self._chunks.append(data)
            if pad4(filesize):
                self._chunks.append(b"\x00" * pad4(filesize))

        self._ino += 1

    def add_dir(self, name: str, st: os.stat_result | None = None) -> None:
        mode_bits = stat.S_IFDIR | 0o755
        mtime = int(time.time())
        if st is not None:
            mode_bits = stat.S_IFDIR | stat.S_IMODE(st.st_mode)
            mtime = int(st.st_mtime)
        self._add_entry(name, mode_bits, b"", mtime=mtime, nlink=2)

    def add_file(self, src: Path, arcname: str) -> None:
        st = src.lstat()
        mode_bits = stat.S_IFREG | stat.S_IMODE(st.st_mode)
        data = src.read_bytes()
        self._add_entry(arcname, mode_bits, data, mtime=int(st.st_mtime), nlink=1)

    def add_symlink(self, src: Path, arcname: str) -> None:
        st = src.lstat()
        target = os.readlink(src)
        data = target.encode("utf-8")
        mode_bits = stat.S_IFLNK | 0o777
        self._add_entry(arcname, mode_bits, data, mtime=int(st.st_mtime), nlink=1)

    def build_from_dir(self, root: Path) -> bytes:
        if not root.exists():
            die(f"rootfs directory not found: {root}")
        if not root.is_dir():
            die(f"rootfs path is not a directory: {root}")

        # Walk in deterministic order.
        for current, dirnames, filenames in os.walk(root, topdown=True, followlinks=False):
            current_path = Path(current)
            rel_current = current_path.relative_to(root)

            if rel_current != Path("."):
                st = current_path.lstat()
                self.add_dir(rel_current.as_posix(), st=st)

            # Handle subdirectories first.
            dirnames_sorted = sorted(dirnames)
            keep_dirs: list[str] = []
            for d in dirnames_sorted:
                p = current_path / d
                rel = p.relative_to(root).as_posix()
                if p.is_symlink():
                    self.add_symlink(p, rel)
                else:
                    self.add_dir(rel, st=p.lstat())
                    keep_dirs.append(d)
            dirnames[:] = keep_dirs

            # Files
            for fname in sorted(filenames):
                p = current_path / fname
                rel = p.relative_to(root).as_posix()
                if p.is_symlink():
                    self.add_symlink(p, rel)
                else:
                    self.add_file(p, rel)

        # End of archive
        self._chunks.append(b"070701")
        self._chunks.append(b"00000000" * 13)  # rest of the header fields
        trailer_name = b"TRAILER!!!\x00"
        self._chunks.append(trailer_name)
        if pad4(len("TRAILER!!!") + 1):
            self._chunks.append(b"\x00" * pad4(len("TRAILER!!!") + 1))

        return b"".join(self._chunks)


# ----------------------------
# GRUB config
# ----------------------------

def make_grub_cfg(kernel_name: str, module_name: str | None) -> str:
    lines = [
        "set timeout=0",
        "set default=0",
        "",
        "menuentry \"CrisOS v3\" {",
        f"    multiboot /boot/{kernel_name}",
    ]
    if module_name:
        lines.append(f"    module /boot/{module_name} rootfs.cpio")
    lines.extend([
        "    boot",
        "}",
        "",
    ])
    return "\n".join(lines)


# ----------------------------
# ISO builder
# ----------------------------

def build_iso(
    kernel: Path,
    rootfs: Path | None,
    output_iso: Path,
    workdir: Path,
    kernel_name: str = "kernel.bin",
    rootfs_name: str = "rootfs.cpio",
    keep_tree: bool = False,
) -> Path:
    grub_mkrescue = which_or_none("grub-mkrescue", "grub2-mkrescue")
    if not grub_mkrescue:
        die("grub-mkrescue/grub2-mkrescue not found in PATH")

    xorriso = which_or_none("xorriso")
    if not xorriso:
        die("xorriso not found in PATH (required by grub-mkrescue)")

    if not kernel.exists():
        die(f"kernel not found: {kernel}")
    if not kernel.is_file():
        die(f"kernel is not a file: {kernel}")

    iso_root = workdir / "iso"
    boot_dir = iso_root / "boot"
    grub_dir = boot_dir / "grub"

    if iso_root.exists() and not keep_tree:
        rm_tree(iso_root)

    boot_dir.mkdir(parents=True, exist_ok=True)
    grub_dir.mkdir(parents=True, exist_ok=True)

    # Copy kernel
    copy_file(kernel, boot_dir / kernel_name)

    # Build rootfs.cpio if provided
    module_name: str | None = None
    if rootfs is not None:
        if not rootfs.exists():
            die(f"rootfs path not found: {rootfs}")
        if not rootfs.is_dir():
            die(f"rootfs is not a directory: {rootfs}")

        info(f"packing rootfs -> {boot_dir / rootfs_name}")
        cpio = NewcBuilder().build_from_dir(rootfs)
        (boot_dir / rootfs_name).write_bytes(cpio)
        module_name = rootfs_name

    # Write grub.cfg
    grub_cfg = make_grub_cfg(kernel_name=kernel_name, module_name=module_name)
    write_text(grub_dir / "grub.cfg", grub_cfg)

    # Build ISO
    ensure_parent(output_iso)
    if output_iso.exists():
        output_iso.unlink()

    cmd = [
        grub_mkrescue,
        "-o",
        str(output_iso),
        str(iso_root),
    ]

    info("building ISO with GRUB...")
    info(" ".join(cmd))
    subprocess.run(cmd, check=True)

    if not output_iso.exists():
        die("ISO build finished but output file was not created")

    return output_iso


def run_qemu(iso: Path, extra_args: list[str] | None = None) -> None:
    qemu = which_or_none("qemu-system-i386")
    if not qemu:
        die("qemu-system-i386 not found in PATH")

    cmd = [
        qemu,
        "-cdrom",
        str(iso),
        "-m",
        "512M",
    ]
    if extra_args:
        cmd.extend(extra_args)

    info("running QEMU...")
    info(" ".join(cmd))
    subprocess.run(cmd, check=True)


# ----------------------------
# CLI
# ----------------------------

def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Build a GRUB Multiboot ISO for CrisOS v3."
    )
    p.add_argument(
        "--kernel",
        default="build/kernel.bin",
        help="Path to kernel.bin (default: build/kernel.bin)",
    )
    p.add_argument(
        "--rootfs",
        default="rootfs",
        help="Path to rootfs directory (default: rootfs). Use --no-rootfs to skip.",
    )
    p.add_argument(
        "--no-rootfs",
        action="store_true",
        help="Do not include a rootfs module.",
    )
    p.add_argument(
        "--output",
        default="os.iso",
        help="Output ISO path (default: os.iso)",
    )
    p.add_argument(
        "--workdir",
        default="build",
        help="Working directory for generated ISO tree (default: build)",
    )
    p.add_argument(
        "--kernel-name",
        default="kernel.bin",
        help="Name used inside the ISO for the kernel (default: kernel.bin)",
    )
    p.add_argument(
        "--rootfs-name",
        default="rootfs.cpio",
        help="Name used inside the ISO for the rootfs module (default: rootfs.cpio)",
    )
    p.add_argument(
        "--keep-tree",
        action="store_true",
        help="Keep the generated iso/ tree in the workdir.",
    )
    p.add_argument(
        "--run",
        action="store_true",
        help="Boot the generated ISO in QEMU after building.",
    )
    p.add_argument(
        "--qemu-arg",
        action="append",
        default=[],
        help="Extra argument passed to qemu-system-i386. Can be repeated.",
    )
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    kernel = Path(args.kernel).resolve()
    output = Path(args.output).resolve()
    workdir = Path(args.workdir).resolve()

    rootfs = None if args.no_rootfs else Path(args.rootfs).resolve()

    workdir.mkdir(parents=True, exist_ok=True)

    try:
        iso = build_iso(
            kernel=kernel,
            rootfs=rootfs,
            output_iso=output,
            workdir=workdir,
            kernel_name=args.kernel_name,
            rootfs_name=args.rootfs_name,
            keep_tree=args.keep_tree,
        )
        info(f"ISO created: {iso} ({iso.stat().st_size} bytes)")

        if args.run:
            run_qemu(iso, extra_args=args.qemu_arg)

        return 0

    except subprocess.CalledProcessError as e:
        die(f"subprocess failed with exit code {e.returncode}")
    except KeyboardInterrupt:
        die("interrupted by user", code=130)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
