from __future__ import annotations
from dataclasses import dataclass, field


@dataclass
class UserConfig:
    username: str = ""
    password: str = ""


@dataclass
class PartitionConfig:
    device: str = ""
    fstype: str = "ext4"
    mountpoint: str = "/mnt/nucleos"
    boot_device: str = ""
    uefi: bool = False
    auto_partition: bool = True


@dataclass
class NucleOSConfig:
    kernel_source: str = ""
    rootfs_source: str = ""
    hostname: str = "nucleos"
    root_password: str = ""
    users: list[UserConfig] | None = None
    install_lcp_repos: bool = True
    partition: PartitionConfig | None = None

    def summary_lines(self) -> list[str]:
        p = self.partition
        l = [
            f"Disco:     {p.device if p else 'N/A'}",
            f"S. arch.:  {p.fstype if p else 'N/A'}",
            f"UEFI:      {'Sí' if p and p.uefi else 'No'}",
            f"Part. auto: {'Sí' if p and p.auto_partition else 'No'}",
            f"Hostname:  {self.hostname}",
            f"Root pass: {'✓' if self.root_password else '✗'}",
        ]
        if self.users:
            l.extend(f"Usuario:   {u.username}" for u in self.users)
        return l
