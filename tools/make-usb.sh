#!/usr/bin/env bash
set -euo pipefail

#
# make-usb.sh — Crea un USB booteable con NucleOS + instalador Python
#
# Modo de uso:
#   sudo bash tools/make-usb.sh /dev/sdX
#
# ADVERTENCIA: Esto BORRA todos los datos del dispositivo.
#
# Crea:
#   - Partición 1: FAT32 (GRUB + kernel + rootfs + instalador)
#   - Esquema: GPT (compatible BIOS y UEFI)
#   - GRUB: i386-pc (MBR) + x86_64-efi
#

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ISO_DIR="${PROJECT_DIR}/iso"
BUILD_DIR="${PROJECT_DIR}/build"
TOOLS_DIR="${PROJECT_DIR}/tools"
KERNEL="${BUILD_DIR}/kernel.bin"
ROOTFS="${ISO_DIR}/boot/rootfs.bin"

RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BLUE='\033[34m'
BOLD='\033[1m'
RESET='\033[0m'

info()  { echo -e "  ${BLUE}::${RESET} $*"; }
ok()    { echo -e "  ${GREEN}✓${RESET} $*"; }
warn()  { echo -e "  ${YELLOW}⚠${RESET} $*"; }
error() { echo -e "  ${RED}✗${RESET} $*" >&2; }

cleanup() {
    if [ -n "${MOUNT_DIR:-}" ]; then
        umount -R "${MOUNT_DIR}" 2>/dev/null || true
        rmdir "${MOUNT_DIR}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

die() {
    error "$*"
    exit 1
}

confirm() {
    echo -en "  ${YELLOW}?${RESET} $* ${BOLD}[y/N]${RESET} "
    read -r resp
    [[ "${resp,,}" == "y" ]] || [[ "${resp,,}" == "yes" ]]
}

# --- Verificaciones ---

[[ $EUID -eq 0 ]] || die "Ejecutar como root: sudo bash $0 /dev/sdX"

[[ $# -ne 1 ]] && die "Uso: sudo bash $0 /dev/sdX (ej: /dev/sdb)"

DEVICE="$1"
[[ ! -b "$DEVICE" ]] && die "No es un dispositivo de bloque: $DEVICE"

# Verificar que no sea el disco del sistema
if mount | grep -q "^${DEVICE}"; then
    die "El dispositivo ${DEVICE} está montado. Desmontar antes de continuar."
fi
if grep -q "${DEVICE}" /proc/mounts 2>/dev/null; then
    die "${DEVICE} está en uso"
fi

# --- Verificar que el kernel y rootfs estén compilados ---

if [[ ! -f "$KERNEL" ]]; then
    warn "Kernel no encontrado en ${KERNEL}"
    if confirm "¿Compilar kernel ahora?"; then
        make -C "$PROJECT_DIR" iso || die "Error compilando kernel"
    else
        die "Compila primero: make iso"
    fi
fi

if [[ ! -f "$ROOTFS" ]]; then
    warn "Rootfs no encontrado en ${ROOTFS}"
    make -C "$PROJECT_DIR" iso || die "Error compilando rootfs"
fi

ok "Kernel: ${KERNEL}"
ok "Rootfs: ${ROOTFS}"

# --- Confirmación ---

echo ""
echo -e "  ${BOLD}Destino:${RESET}  ${DEVICE}"
echo -e "  ${BOLD}Tamaño:${RESET}   $(lsblk -ndo SIZE "${DEVICE}")"
echo -e "  ${BOLD}Modelo:${RESET}   $(lsblk -ndo MODEL "${DEVICE}" 2>/dev/null || echo 'Desconocido')"
echo ""
confirm "¿BORRAR todos los datos en ${DEVICE} y crear USB booteable?" || die "Cancelado por el usuario"

# --- Desmontar y limpiar ---

MOUNT_DIR=$(mktemp -d /tmp/nucleos-usb.XXXXXX)
info "Desmontando particiones existentes..."
for part in $(lsblk -nlo NAME "${DEVICE}" | tail -n +2); do
    umount "/dev/${part}" 2>/dev/null || true
done
swapoff "${DEVICE}"* 2>/dev/null || true

# --- Particionado ---

info "Particionando ${DEVICE} (GPT)..."
wipefs --all --force "${DEVICE}"
sgdisk --zap-all --clear "${DEVICE}"

# BIOS boot partition (GRUB core.img)
sgdisk --new=1:2048:+2M --typecode=1:EF02 --change-name=1:"GRUB" "${DEVICE}"

# EFI System Partition (FAT32)
sgdisk --new=2:0:+512M --typecode=2:EF00 --change-name=2:"EFI" "${DEVICE}"

# Data partition (kernel + rootfs + installer)
sgdisk --new=3:0:0 --typecode=3:8300 --change-name=3:"NUCLEOS" "${DEVICE}"

partprobe "${DEVICE}"
sleep 1

# Determinar nombres de particiones
if echo "${DEVICE}" | grep -q "nvme"; then
    PART1="${DEVICE}p1"
    PART2="${DEVICE}p2"
    PART3="${DEVICE}p3"
elif echo "${DEVICE}" | grep -q "mmcblk"; then
    PART1="${DEVICE}p1"
    PART2="${DEVICE}p2"
    PART3="${DEVICE}p3"
else
    PART1="${DEVICE}1"
    PART2="${DEVICE}2"
    PART3="${DEVICE}3"
fi

ok "Particiones creadas: ${PART1} (BIOS), ${PART2} (EFI), ${PART3} (Datos)"

# --- Formateo ---

info "Formateando particiones..."
# BIOS boot: no se formatea, solo se usa para GRUB core.img
# EFI: FAT32
mkfs.fat -F32 -n "NUCLEOS_EFI" "${PART2}" >/dev/null
ok "EFI partición formateada: ${PART2} (FAT32)"
# Data: ext4
mkfs.ext4 -F -L "NUCLEOS_DATA" "${PART3}" >/dev/null
ok "Datos partición formateada: ${PART3} (ext4)"

# --- Copiar archivos ---

info "Montando partición de datos..."
mount "${PART3}" "${MOUNT_DIR}"

info "Instalando GRUB..."
grub-install \
    --target=i386-pc \
    --boot-directory="${MOUNT_DIR}/boot" \
    --recheck \
    "${DEVICE}" >/dev/null 2>&1 || warn "GRUB BIOS instalación falló (puede ignorarse si usas UEFI)"

grub-install \
    --target=x86_64-efi \
    --efi-directory="${MOUNT_DIR}" \
    --boot-directory="${MOUNT_DIR}/boot" \
    --bootloader-id="NucleOS" \
    --recheck \
    --removable >/dev/null 2>&1 || warn "GRUB EFI instalación falló (puede ignorarse si usas BIOS)"

ok "GRUB instalado"

# Copiar kernel y rootfs
mkdir -p "${MOUNT_DIR}/boot/grub"
cp "${KERNEL}" "${MOUNT_DIR}/boot/kernel.bin"
cp "${ROOTFS}" "${MOUNT_DIR}/boot/rootfs.bin"
ok "Kernel + rootfs copiados"

# Copiar instalador
if [[ -d "${TOOLS_DIR}/installer" ]]; then
    mkdir -p "${MOUNT_DIR}/installer"
    cp -r "${TOOLS_DIR}/installer"/* "${MOUNT_DIR}/installer/"
    cp "${TOOLS_DIR}/nucleos-install" "${MOUNT_DIR}/installer/"
    cp "${TOOLS_DIR}/lcp.py" "${TOOLS_DIR}/lcp_main_repo.json" "${MOUNT_DIR}/installer/" 2>/dev/null || true
    rm -rf "${MOUNT_DIR}/installer/__pycache__"
    ok "Instalador copiado a /installer"

    # README en la raíz
    cat > "${MOUNT_DIR}/LEEME.txt" << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║                    NucleOS v3.0.0                        ║
║           USB Booteable + Instalador                     ║
╚═══════════════════════════════════════════════════════════╝

Para INSTALAR NucleOS en tu disco duro:

  1. Arranca un Linux Live USB (Arch Linux, Ubuntu, etc.)
  2. Conecta este USB y verifica el dispositivo:
       lsblk
  3. Monta la partición de datos (NUCLEOS_DATA):
       sudo mount /dev/sdX3 /mnt
  4. Ejecuta el instalador:
       sudo python /mnt/installer/nucleos-install

Para ARRANCAR NucleOS directamente:
  - Simplemente bootea desde este USB
  - Selecciona "NucleOS v3 (Boot)" en GRUB

Requisitos del instalador:
  - python3
  - grub-install, grub-mkrescue
  - sfdisk, mkfs.ext4, mkfs.fat
EOF
else
    warn "Directorio installer/ no encontrado, omitiendo"
fi

# --- GRUB config ---
cat > "${MOUNT_DIR}/boot/grub/grub.cfg" << 'GRUB'
set timeout=10
set default=0

insmod all_video
insmod gfxterm
insmod font
insmod ext2

set gfxmode=auto
set gfxpayload=text

if loadfont $prefix/fonts/unicode.pf2; then
  set gfxmode=1024x768
  terminal_output gfxterm
fi

menuentry "NucleOS v3 (Boot)" {
    insmod ext2
    set root=(hd0,msdos3)
    multiboot /boot/kernel.bin
    module  /boot/rootfs.bin rootfs
    boot
}

menuentry "NucleOS v3 (Install — información)" {
    echo "────────────────────────────────────────────────"
    echo "  Para INSTALAR NucleOS en tu disco duro:"
    echo ""
    echo "  1. Arranca desde un Linux Live USB"
    echo "  2. Monta la partición NUCLEOS_DATA:"
    echo "       sudo mount /dev/sdX3 /mnt"
    echo "  3. Ejecuta el instalador:"
    echo "       sudo python /mnt/installer/nucleos-install"
    echo "────────────────────────────────────────────────"
    echo ""
    echo "Presiona cualquier tecla para reiniciar..."
    boot
}

menuentry "Reiniciar" {
    reboot
}

menuentry "Apagar" {
    halt
}
GRUB

ok "GRUB config escrita"

# --- Sincronizar ---

sync
ok "Sincronizado"

# --- Resumen ---

echo ""
echo -e "${GREEN}${BOLD}╔═══════════════════════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}${BOLD}║           USB Booteable creado exitosamente              ║${RESET}"
echo -e "${GREEN}${BOLD}╚═══════════════════════════════════════════════════════════╝${RESET}"
echo ""
echo -e "  ${BOLD}Dispositivo:${RESET}  ${DEVICE}"
echo -e "  ${BOLD}GRUB:${RESET}         BIOS (i386-pc) + EFI (x86_64)"
echo -e "  ${BOLD}Kernel:${RESET}       ${KERNEL}"
echo -e "  ${BOLD}Rootfs:${RESET}       ${ROOTFS}"
echo -e "  ${BOLD}Instalador:${RESET}   /installer/nucleos-install (en el USB)"
echo ""
echo -e "  ${YELLOW}Para instalar NucleOS en otro disco:${RESET}"
echo -e "    sudo python ${MOUNT_DIR}/installer/nucleos-install"
echo ""
