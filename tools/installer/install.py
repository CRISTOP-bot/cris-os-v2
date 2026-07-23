from __future__ import annotations
import os, shutil, stat, struct, subprocess
from pathlib import Path

from installer.config import NucleOSConfig
from installer.disk import create_partitions, format_partition, mount, unmount
from installer.ui import ok, info, warn, error

MAGIC, VERSION = b"CRFS", 1
HDR = "<4sIII"
ENT = "<64sII"
HDR_SZ = struct.calcsize(HDR)
ENT_SZ = struct.calcsize(ENT)


def _build_rootfs(rootfs_dir: str, output: str) -> None:
    files = []
    for root, _, fnames in os.walk(rootfs_dir):
        for f in sorted(fnames):
            p = Path(root) / f
            files.append((str(p.relative_to(rootfs_dir)), p.read_bytes()))

    offset = (HDR_SZ + len(files) * ENT_SZ + 3) & ~3
    entries, content = [], b""
    for name, data in files:
        padded = name.encode()[:63].ljust(64, b"\x00")
        entries.append(struct.pack(ENT, padded, offset, len(data)))
        content += data + b"\x00" * ((-len(data)) & 3)
        offset += len(data) + ((-len(data)) & 3)

    with open(output, "wb") as f:
        f.write(struct.pack(HDR, MAGIC, VERSION, len(files), 0))
        for e in entries:
            f.write(e)
        pad = (-f.tell()) & 3
        if pad:
            f.write(b"\x00" * pad)
        f.write(content)


GRUB_CFG = """set timeout=5
set default=0

insmod all_video
insmod {theme}

set gfxmode=auto
set gfxpayload=text

menuentry "NucleOS" {{
    insmod ext2
    set root=(hd0,{root})
    multiboot /boot/kernel.bin
    module  /boot/rootfs.bin rootfs
    boot
}}
"""


def _grub_install(target: str, disk: str, uefi: bool) -> None:
    target = target.rstrip("/")
    if not shutil.which("grub-install"):
        raise RuntimeError("grub-install no encontrado")

    if uefi:
        subprocess.run(["grub-install", "--target=x86_64-efi",
                        "--efi-directory", f"{target}/boot",
                        "--bootloader-id=NucleOS", "--recheck"],
                       check=True, capture_output=True)
        cfg = GRUB_CFG.format(theme="gfxterm", root="msdos2")
    else:
        subprocess.run(["grub-install", "--target=i386-pc", "--recheck", disk],
                       check=True, capture_output=True)
        cfg = GRUB_CFG.format(theme="theme", root="msdos2")

    (Path(target) / "boot" / "grub").mkdir(parents=True, exist_ok=True)
    (Path(target) / "boot" / "grub" / "grub.cfg").write_text(cfg)
    ok(f"GRUB instalado")


def _configure(target: str, config: NucleOSConfig) -> None:
    etc = Path(target) / "etc"
    etc.mkdir(parents=True, exist_ok=True)

    (etc / "hostname").write_text(f"{config.hostname}\n")
    (etc / "os-release").write_text(
        f"""NAME="NucleOS"\nVERSION="3.0.0"\nID=nucleos\nPRETTY_NAME="NucleOS v3"\nHOME_URL="https://github.com/nucleos/nucleos"\n"""
    )
    (etc / "issue").write_text(f"NucleOS v3.0.0 \\n \\l ({config.hostname})\n")

    passwd = f"root:{config.root_password or ''}:0:0:root:/root:/bin/sh\n"
    if config.users:
        passwd += "\n".join(
            f"{u.username}:{u.password}:1000:1000:{u.username}:/home/{u.username}:/bin/sh"
            for u in config.users
        )
        passwd += "\n"
    (etc / "passwd").write_text(passwd)
    info("Configuración aplicada")


def _setup(partition) -> dict:
    if partition.auto_partition:
        info(f"Particionando {partition.device}...")
        parts = create_partitions(partition.device, partition.uefi)
        _probe(partition.device)
        boot_part = sorted(parts.items())[0][1]
        root_part = sorted(parts.items())[1][1]

        fmt = "vfat" if partition.uefi else "ext2"
        format_partition(boot_part, fmt, "NUCLEOS_BOOT")
        format_partition(root_part, partition.fstype, "NUCLEOS_ROOT")
        return {"boot": boot_part, "root": root_part}

    subprocess.run(["partprobe", partition.device], capture_output=True, check=True)
    return {"boot": partition.boot_device, "root": partition.device}


def _probe(disk: str) -> None:
    subprocess.run(["partprobe", disk], capture_output=True, check=True)


def install(config: NucleOSConfig) -> bool:
    p = config.partition
    if not p or not os.path.exists(p.device):
        error("Sin configuración de partición o dispositivo no existe")
        return False

    mnt = p.mountpoint
    try:
        parts = _setup(p)

        os.makedirs(mnt, exist_ok=True)
        mount(parts["root"], mnt)

        bmnt = Path(mnt) / "boot"
        bmnt.mkdir(exist_ok=True)
        if parts["boot"] != parts["root"]:
            mount(parts["boot"], str(bmnt))

        for d in ("boot", "boot/grub", "dev", "etc", "home", "proc", "sys", "tmp", "root"):
            (Path(mnt) / d).mkdir(parents=True, exist_ok=True)

        if os.path.isfile(config.kernel_source):
            shutil.copy2(config.kernel_source, bmnt / "kernel.bin")
            ok("Kernel copiado")
        else:
            warn("Kernel no encontrado")

        if os.path.isdir(config.rootfs_source):
            _build_rootfs(config.rootfs_source, str(bmnt / "rootfs.bin"))
            ok("Rootfs empaquetado")
        else:
            warn("Rootfs no encontrado")

        _grub_install(mnt, p.device, p.uefi)
        _configure(mnt, config)

        dev = Path(mnt) / "dev"
        dev.mkdir(exist_ok=True)
        try:
            os.mknod(str(dev / "null"), mode=0o666 | stat.S_IFCHR, device=os.makedev(1, 3))
        except OSError:
            pass

        ok("Instalación completada")
        return True

    except subprocess.CalledProcessError as e:
        error(f"Error: {e}")
        if e.stdout:
            error(e.stdout.decode()[:500])
        if e.stderr:
            error(e.stderr.decode()[:500])
        return False
    except Exception as e:
        error(f"Error: {e}")
        return False
    finally:
        unmount(mnt)
