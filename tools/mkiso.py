#!/usr/bin/env python3
from __future__ import annotations
import argparse, os, shutil, stat, subprocess, sys, time
from pathlib import Path
from typing import Iterable, Iterator


def die(msg: str, code: int = 1) -> None:
    print(f"[mkiso] error: {msg}", file=sys.stderr); raise SystemExit(code)
def info(msg: str) -> None:
    print(f"[mkiso] {msg}")
def which(*names: str) -> str | None:
    for n in names:
        if p := shutil.which(n):
            return p
    return None
def pad4(n: int) -> int:
    return (-n) % 4


class NewcBuilder:
    def __init__(self) -> None:
        self._ino = 1
        self._chunks: list[bytes] = []

    def _h8(self, n: int) -> bytes:
        return f"{n & 0xFFFFFFFF:08x}".encode()

    def _add(self, name: str, mode: int, data: bytes = b"", mtime: int | None = None,
             nlink: int = 1, uid: int = 0, gid: int = 0, **kw) -> None:
        mtime = mtime or int(time.time())
        nb = name.encode()
        ns, fs = len(nb) + 1, len(data)
        h = b"".join([b"070701", *(self._h8(v) for v in
            (self._ino, mode, uid, gid, nlink, mtime, fs, 0, 0, 0, 0, ns, 0))])
        self._chunks += [h, nb + b"\x00", b"\x00" * pad4(ns)]
        if fs:
            self._chunks += [data, b"\x00" * pad4(fs)]
        self._ino += 1

    def add_dir(self, name: str, st: os.stat_result | None = None) -> None:
        m, t = stat.S_IFDIR | 0o755, int(time.time())
        if st:
            m, t = stat.S_IFDIR | stat.S_IMODE(st.st_mode), int(st.st_mtime)
        self._add(name, m, mtime=t, nlink=2)

    def add_file(self, src: Path, arc: str) -> None:
        st = src.lstat()
        self._add(arc, stat.S_IFREG | stat.S_IMODE(st.st_mode), src.read_bytes(),
                  mtime=int(st.st_mtime))

    def add_symlink(self, src: Path, arc: str) -> None:
        self._add(arc, stat.S_IFLNK | 0o777, os.readlink(src).encode(),
                  mtime=int(src.lstat().st_mtime))

    def build(self, root: Path) -> bytes:
        if not root.is_dir():
            die(f"rootfs no encontrado: {root}")
        for cur, dirs, files in os.walk(root, topdown=True, followlinks=False):
            cp = Path(cur)
            rel = cp.relative_to(root)
            if rel != Path("."):
                self.add_dir(rel.as_posix(), st=cp.lstat())
            for d in sorted(dirs):
                p = cp / d
                r = p.relative_to(root).as_posix()
                (self.add_symlink if p.is_symlink() else self.add_dir)(p, r)
            dirs[:] = [d for d in sorted(dirs) if not (cp / d).is_symlink()]
            for f in sorted(files):
                p = cp / f
                (self.add_symlink if p.is_symlink() else self.add_file)(p, p.relative_to(root).as_posix())
        self._chunks += [b"070701", b"00000000" * 13, b"TRAILER!!!\x00",
                         b"\x00" * pad4(len("TRAILER!!!") + 1)]
        return b"".join(self._chunks)


def grub_cfg(kernel: str, module: str | None, has_installer: bool) -> str:
    lines = ["set timeout=10", "set default=0", ""]
    if has_installer:
        lines += [
            f'menuentry "NucleOS (Boot)" {{',
            f"    multiboot /boot/{kernel}",
            f"    module /boot/{module} rootfs.cpio" if module else "",
            "    boot",
            "}",
            "",
            'menuentry "NucleOS (Install — info)" {',
            '    echo "────────────────────────────────────────────────"',
            '    echo "  Para INSTALAR NucleOS:"',
            '    echo "  1. Arranca un Linux Live USB"',
            '    echo "  2. Monta este USB: mount /dev/sdX1 /mnt"',
            '    echo "  3. Ejecuta: sudo python /mnt/installer/nucleos-install"',
            '    echo "────────────────────────────────────────────────"',
            '    echo ""',
            '    echo "Presiona cualquier tecla..."',
            "    boot",
            "}",
        ]
    else:
        lines += [
            f'menuentry "NucleOS" {{',
            f"    multiboot /boot/{kernel}",
            f"    module /boot/{module} rootfs.cpio" if module else "",
            "    boot",
            "}",
        ]
    return "\n".join(filter(None, lines))


def copy_installer(src: Path, dst: Path) -> None:
    target = dst / "installer"
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(src, target, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    (target / "README.txt").write_text(
        "NucleOS Installer\n"
        "=================\n"
        "1. Arranca un Linux Live USB\n"
        "2. Monta este USB: mount /dev/sdX1 /mnt\n"
        "3. Instala: sudo python /mnt/installer/nucleos-install\n"
    )
    info(f"Instalador → {target}")


def build_iso(kernel: Path, rootfs: Path | None, output: Path, workdir: Path,
              kernel_name: str = "kernel.bin", rootfs_name: str = "rootfs.cpio",
              keep_tree: bool = False, installer: Path | None = None) -> Path:
    if not which("grub-mkrescue", "grub2-mkrescue"):
        die("grub-mkrescue no encontrado")
    if not which("xorriso"):
        die("xorriso no encontrado")
    if not kernel.is_file():
        die(f"kernel no encontrado: {kernel}")

    iso_root = workdir / "iso"
    boot_dir, grub_dir = iso_root / "boot", iso_root / "boot/grub"
    if iso_root.exists() and not keep_tree:
        shutil.rmtree(iso_root)
    boot_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(kernel, boot_dir / kernel_name)

    module = None
    if rootfs and rootfs.is_dir():
        info(f"Empaquetando rootfs → {rootfs_name}")
        (boot_dir / rootfs_name).write_bytes(NewcBuilder().build(rootfs))
        module = rootfs_name

    has_installer = False
    if installer and installer.is_dir():
        copy_installer(installer, iso_root)
        has_installer = True

    (grub_dir / "grub.cfg").write_text(grub_cfg(kernel_name, module, has_installer))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    subprocess.run([which("grub-mkrescue", "grub2-mkrescue"), "-o", str(output), str(iso_root)], check=True)
    if not output.exists():
        die("ISO no creada")
    return output


def run_qemu(iso: Path, args: list[str] | None = None) -> None:
    q = which("qemu-system-x86_64", "qemu-system-i386")
    if not q:
        die("qemu no encontrado")
    subprocess.run([q, "-cdrom", str(iso), "-m", "512M", *(args or [])], check=True)


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Build NucleOS ISO")
    p.add_argument("--kernel", default="build/kernel.bin")
    p.add_argument("--rootfs", default="rootfs")
    p.add_argument("--no-rootfs", action="store_true")
    p.add_argument("--output", default="os.iso")
    p.add_argument("--workdir", default="build")
    p.add_argument("--kernel-name", default="kernel.bin")
    p.add_argument("--rootfs-name", default="rootfs.cpio")
    p.add_argument("--keep-tree", action="store_true")
    p.add_argument("--run", action="store_true")
    p.add_argument("--qemu-arg", action="append", default=[])
    p.add_argument("--installer", default=None)
    p.add_argument("--no-installer", action="store_true")
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    kernel = Path(args.kernel).resolve()
    output = Path(args.output).resolve()
    workdir = Path(args.workdir).resolve()
    rootfs = None if args.no_rootfs else Path(args.rootfs).resolve()
    installer = None
    if not args.no_installer:
        inst = Path(args.installer).resolve() if args.installer else Path(__file__).parent / "installer"
        if inst.exists():
            installer = inst
    workdir.mkdir(parents=True, exist_ok=True)
    try:
        iso = build_iso(kernel, rootfs, output, workdir, args.kernel_name,
                        args.rootfs_name, args.keep_tree, installer)
        info(f"ISO: {iso} ({iso.stat().st_size} bytes)")
        if args.run:
            run_qemu(iso, args.qemu_arg)
        return 0
    except subprocess.CalledProcessError as e:
        die(f"subprocess falló ({e.returncode})")
    except KeyboardInterrupt:
        die("interrumpido", 130)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
