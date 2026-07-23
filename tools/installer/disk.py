from __future__ import annotations
import os, re, subprocess
from dataclasses import dataclass, field
from pathlib import Path

SYS = Path("/sys/block")


@dataclass
class Partition:
    path: str
    size: int
    fstype: str


@dataclass
class Disk:
    path: str
    model: str
    size: int
    rotational: bool
    partitions: list[Partition] = field(default_factory=list)


def _read(path: Path) -> str:
    try:
        return path.read_text().strip()
    except OSError:
        return ""


def _sysfs_attr(dev: str, attr: str) -> str:
    return _read(SYS / dev / attr)


def detect_disks() -> list[Disk]:
    pattern = re.compile(r"^(sd[a-z]|nvme\d+n\d+|vd[a-z]|mmcblk\d+)$")
    disks = []
    for entry in SYS.iterdir():
        if not pattern.match(entry.name):
            continue
        size = int(_sysfs_attr(entry.name, "size") or 0) * 512
        if not size:
            continue
        model = _read(entry / "device" / "model") or _read(entry / "dm" / "name") or entry.name
        rot = _sysfs_attr(entry.name, "queue/rotational") == "1"
        disk = Disk(path=f"/dev/{entry.name}", model=model, size=size, rotational=rot)
        disk.partitions = _detect_partitions(entry.name)
        disks.append(disk)
    return disks


def _detect_partitions(dev: str) -> list[Partition]:
    parts = []
    base = SYS / dev
    if not base.is_dir():
        return parts
    for entry in sorted(base.iterdir()):
        if not entry.is_dir() or entry.name == dev or not entry.name.startswith(dev):
            continue
        size = int(_read(entry / "size") or 0) * 512
        fstype = _blkid(f"/dev/{entry.name}")
        parts.append(Partition(path=f"/dev/{entry.name}", size=size, fstype=fstype))
    return parts


def _blkid(dev: str) -> str:
    try:
        r = subprocess.run(["blkid", "-o", "value", "-s", "TYPE", dev],
                           capture_output=True, text=True, timeout=10)
        return r.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return ""


def format_size(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n //= 1024
    return f"{n:.1f} PB"


def _sfdisk(disk: str, input: str = "") -> None:
    subprocess.run(["sfdisk", "--force", disk],
                   input=input.encode(), capture_output=True, check=True)


def _wipe(disk: str) -> None:
    subprocess.run(["wipefs", "--all", "--force", disk], capture_output=True, check=True)
    subprocess.run(["sgdisk", "--zap-all", "--clear", disk], capture_output=True, check=True)


def _probe(disk: str) -> None:
    subprocess.run(["partprobe", disk], capture_output=True, check=True)


FS_MAP = {
    "vfat": (["mkfs.fat", "-F32"], ["-n"]),
    "ext4": (["mkfs.ext4", "-F"], ["-L"]),
    "ext3": (["mkfs.ext3", "-F"], ["-L"]),
    "ext2": (["mkfs.ext2", "-F"], ["-L"]),
    "btrfs": (["mkfs.btrfs", "-f"], ["-L"]),
    "swap": (["mkswap"], []),
}


def format_partition(part: str, fstype: str, label: str = "") -> None:
    cmd, flag = FS_MAP.get(fstype, (["mkfs.ext4", "-F"], ["-L"]))
    if label and flag:
        cmd = [*cmd, *flag, label]
    cmd.append(part)
    subprocess.run(cmd, capture_output=True, check=True)


def create_partitions(disk: str, uefi: bool) -> dict[str, str]:
    _wipe(disk)
    if uefi:
        layout = (
            "label: gpt\nunit: sectors\n"
            "start=2048, size=+512M, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\n"
            "size=+, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4\n"
        )
    else:
        layout = (
            "label: dos\nunit: sectors\n"
            "start=2048, size=+1M, type=83, bootable\n"
            "size=+, type=83\n"
        )
    _sfdisk(disk, layout)
    _probe(disk)
    return _parse_partitions(disk)


def _parse_partitions(disk: str) -> dict[str, str]:
    r = subprocess.run(["sfdisk", "--dump", disk], capture_output=True, text=True, check=True)
    parts = {}
    for line in r.stdout.splitlines():
        line = line.strip()
        if not line or line.startswith(("#", "label")):
            continue
        if ":" in line and "/dev/" in line:
            parts[f"part{len(parts)}"] = line.split(":")[0].strip()
    return parts


def mount(dev: str, target: str) -> None:
    Path(target).mkdir(parents=True, exist_ok=True)
    subprocess.run(["mount", dev, target], capture_output=True, check=True)


def unmount(target: str) -> None:
    subprocess.run(["umount", "-R", target], capture_output=True)


def is_efi() -> bool:
    return Path("/sys/firmware/efi").is_dir()
