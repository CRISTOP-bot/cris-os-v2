#!/usr/bin/env bash
# ============================================================================
#  NucleOS - Dependency Installer
#  Multi-platform: Linux (Arch, Debian, Fedora, SUSE, Void, Alpine, Gentoo,
#                   NixOS, FreeBSD), macOS, Windows (MSYS2/Git Bash/WSL)
# ============================================================================
set -euo pipefail

# ── Colors ──────────────────────────────────────────────────────────────────
if [[ -t 1 ]] && command -v tput &>/dev/null && [[ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]]; then
    R='\033[0;31m'   # Red
    G='\033[0;32m'   # Green
    Y='\033[0;33m'   # Yellow
    B='\033[0;34m'   # Blue
    M='\033[0;35m'   # Magenta
    C='\033[0;36m'   # Cyan
    W='\033[1;37m'   # White (bold)
    DIM='\033[2m'    # Dim
    BOLD='\033[1m'   # Bold
    RESET='\033[0m'  # Reset
else
    R='' G='' Y='' B='' M='' C='' W='' DIM='' BOLD='' RESET=''
fi

# ── Symbols ─────────────────────────────────────────────────────────────────
OK="${G}[+]${RESET}"
FAIL="${R}[!]${RESET}"
INFO="${C}[*]${RESET}"
WARN="${Y}[~]${RESET}"
ASK="${M}[?]${RESET}"
STAR="${Y}[*]${RESET}"
ARROW="${C}->${RESET}"
CHECK="${G}${BOLD}✓${RESET}"
CROSS="${R}${BOLD}✗${RESET}"

# ── Helpers ─────────────────────────────────────────────────────────────────
banner() {
    echo ""
    echo -e "${C}${BOLD}"
    cat << 'BANNER'
    _   _ _____ _____     _____         _
   | \ | |_   _| ____|   |  ___|_ _ __| | ___  _ __
   |  \| | | | |  _|     | |_ / _` / __| |/ _ \| '_ \
   | |\  | | | | |___    |  _| (_| \__ \ | (_) | | | |
   |_| \_| |_| |_____|   |_|  \__,_|___/_|\___/|_| |_|
BANNER
    echo -e "${RESET}"
    echo -e "${W}   Dependency Installer${RESET}"
    echo -e "${DIM}   Multi-platform • Interactive • Colorful${RESET}"
    echo ""
    echo -e "${DIM}$(printf '─%.0s' {1..56})${RESET}"
    echo ""
}

log_ok()   { echo -e "  ${CHECK}  ${G}$1${RESET}"; }
log_fail() { echo -e "  ${CROSS}  ${R}$1${RESET}"; }
log_info() { echo -e "  ${INFO}  ${B}$1${RESET}"; }
log_warn() { echo -e "  ${WARN}  ${Y}$1${RESET}"; }
log_ask()  { echo -e "  ${ASK}  ${M}$1${RESET}"; }

section() {
    echo ""
    echo -e "  ${C}${BOLD}$1${RESET}"
    echo -e "${DIM}  $(printf '─%.0s' {1..50})${RESET}"
}

ask_confirm() {
    local msg="$1"
    local default="${2:-y}"
    local hint
    if [[ "$default" == "y" ]]; then
        hint="[Y/n]"
    else
        hint="[y/N]"
    fi
    echo ""
    read -rp "$(echo -e "  ${ASK}  ${W}$msg${RESET} $DIM$hint${RESET} ")" answer
    answer="${answer:-$default}"
    [[ "${answer,,}" == "y" || "${answer,,}" == "yes" ]]
}

spinner() {
    local pid=$1
    local msg="${2:-Working...}"
    local chars='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    while kill -0 "$pid" 2>/dev/null; do
        printf "\r  ${C}%s${RESET}  %s" "${chars:i++%${#chars}:1}" "$msg"
        sleep 0.08
    done
    printf "\r%*s\r" $((${#msg} + 6)) ""
}

detect_os() {
    OS=""
    if [[ -n "${MACOS_DETECTED:-}" ]] || [[ "$(uname)" == "Darwin" ]]; then
        OS="macos"
        DISTRO="macos"
        return
    fi
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        DISTRO="${ID:-unknown}"
    elif [[ -f /etc/lsb-release ]]; then
        . /etc/lsb-release
        DISTRO="${DISTRIB_ID:-unknown}"
    elif [[ "$(uname)" == "FreeBSD" ]]; then
        DISTRO="freebsd"
    else
        DISTRO="unknown"
    fi
    OS="linux"
}

detect_pkg_mgr() {
    PKG_MGR=""
    case "$DISTRO" in
        arch|manjaro|endeavouros|garuda)
            PKG_MGR="pacman" ;;
        ubuntu|debian|linuxmint|pop|elementary|zorin|kali|raspbian)
            PKG_MGR="apt" ;;
        fedora)
            PKG_MGR="dnf" ;;
        opensuse*|suse*|sles)
            PKG_MGR="zypper" ;;
        void)
            PKG_MGR="xbps" ;;
        alpine)
            PKG_MGR="apk" ;;
        gentoo|funtoo)
            PKG_MGR="emerge" ;;
        nixos|nix)
            PKG_MGR="nix" ;;
        freebsd)
            PKG_MGR="pkg" ;;
        macos)
            PKG_MGR="brew" ;;
        *)
            if command -v apt &>/dev/null; then
                PKG_MGR="apt"
            elif command -v pacman &>/dev/null; then
                PKG_MGR="pacman"
            elif command -v dnf &>/dev/null; then
                PKG_MGR="dnf"
            elif command -v brew &>/dev/null; then
                PKG_MGR="brew"
            fi
            ;;
    esac
}

# ── Package names per distro ────────────────────────────────────────────────
pkg_names() {
    local pkg="$1"
    case "$PKG_MGR" in
        pacman)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu-full" ;;
                python)     echo "python" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        apt)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub-pc-bin grub-common xorriso mtools" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu-system-x86" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        dnf)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub2-tools xorriso mtools" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu-system-x86" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        zypper)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub2-x86_64 xorriso mtools" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu-x86" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        xbps)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        apk)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub grub-efi xorriso mtools" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu-system-x86_64" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        emerge)
            case "$pkg" in
                gcc)        echo "sys-devel/gcc" ;;
                binutils)   echo "sys-libs/binutils" ;;
                make)       echo "sys-devel/make" ;;
                grub)       echo "sys-boot/grub dev-libs/xorriso dev-util/mtools" ;;
                xorriso)    echo "dev-libs/xorriso" ;;
                mtools)     echo "dev-util/mtools" ;;
                qemu)       echo "app-emulation/qemu" ;;
                python)     echo "dev-lang/python" ;;
                nasm)       echo "dev-lang/nasm" ;;
            esac ;;
        nix)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "gnumake" ;;
                grub)       echo "grub" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu_full" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        pkg)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "gmake" ;;
                grub)       echo "grub2 xorriso mtools" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
        brew)
            case "$pkg" in
                gcc)        echo "gcc" ;;
                binutils)   echo "binutils" ;;
                make)       echo "make" ;;
                grub)       echo "grub" ;;
                xorriso)    echo "xorriso" ;;
                mtools)     echo "mtools" ;;
                qemu)       echo "qemu" ;;
                python)     echo "python3" ;;
                nasm)       echo "nasm" ;;
            esac ;;
    esac
}

# ── Dependency definitions ──────────────────────────────────────────────────
declare -A DEP_NAMES
DEP_NAMES=(
    [gcc]="GNU C Compiler (gcc)"
    [binutils]="GNU Binutils (ld, as)"
    [make]="GNU Make"
    [grub]="GRUB + xorriso + mtools (for ISO creation)"
    [xorriso]="xorriso (ISO 9660 manipulation)"
    [mtools]="mtools (FAT filesystem tools)"
    [qemu]="QEMU x86_64 System Emulator"
    [python]="Python 3 (rootfs builder)"
    [nasm]="Netwide Assembler (nasm)"
)

# Dependencies are categorized for clarity
DEP_LIST_CORE=(gcc binutils make nasm)
DEP_LIST_ISO=(xorriso mtools)
DEP_LIST_QEMU=(qemu)
DEP_LIST_PY=(python)

# ── Check functions ─────────────────────────────────────────────────────────
check_cmd() {
    command -v "$1" &>/dev/null
}

check_gcc()          { check_cmd gcc; }
check_binutils()     { check_cmd ld && check_cmd as; }
check_make()         { check_cmd make || check_cmd gmake; }
check_nasm()         { check_cmd nasm; }
check_xorriso()      { check_cmd xorriso; }
check_mtools()       { check_cmd mformat; }
check_qemu()         { check_cmd qemu-system-x86_64; }
check_python()       { check_cmd python3 || check_cmd python; }
check_grub()         { check_cmd grub-mkrescue; }

# ── Install functions ───────────────────────────────────────────────────────
install_with_pacman() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo pacman -S --noconfirm --needed "${packages[@]}"
}

install_with_apt() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo apt-get update -qq
    sudo apt-get install -y "${packages[@]}"
}

install_with_dnf() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo dnf install -y "${packages[@]}"
}

install_with_zypper() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo zypper --non-interactive install "${packages[@]}"
}

install_with_xbps() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo xbps-install -Sy "${packages[@]}"
}

install_with_apk() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo apk add "${packages[@]}"
}

install_with_emerge() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo emerge -av "${packages[@]}"
}

install_with_nix() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("-p" "$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    nix-env -iA nixpkgs.gnumake "${packages[@]}" 2>/dev/null || nix-env -i "${packages[@]}"
}

install_with_pkg() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    sudo pkg install -y "${packages[@]}"
}

install_with_brew() {
    local packages=()
    for p in "$@"; do
        local names
        names=$(pkg_names "$p")
        for n in $names; do
            packages+=("$n")
        done
    done
    if [[ ${#packages[@]} -eq 0 ]]; then return 0; fi
    brew install "${packages[@]}"
}

do_install() {
    local func="install_with_${PKG_MGR}"
    "$func" "$@"
}

# ── Get list of missing deps ────────────────────────────────────────────────
get_missing() {
    local missing=()
    for dep in "$@"; do
        local func="check_${dep}"
        if ! $func 2>/dev/null; then
            missing+=("$dep")
        fi
    done
    echo "${missing[@]}"
}

# ── Pretty status for a single dep ──────────────────────────────────────────
show_dep_status() {
    local dep="$1"
    local func="check_${dep}"
    local name="${DEP_NAMES[$dep]:-$dep}"
    if $func 2>/dev/null; then
        log_ok "$name"
    else
        log_fail "$name  ${DIM}(not found)${RESET}"
    fi
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    banner

    detect_os
    detect_pkg_mgr

    echo -e "  ${INFO}  Platform:   ${W}${OS}${RESET} / ${W}${DISTRO}${RESET}"
    echo -e "  ${INFO}  Pkg Manager:${W} ${PKG_MGR:-unknown}${RESET}"
    echo ""

    # ── Show status of all deps ─────────────────────────────────────────
    section "Checking Dependencies"

    ALL_DEPS=("${DEP_LIST_CORE[@]}" "${DEP_LIST_ISO[@]}" "${DEP_LIST_PY[@]}" "${DEP_LIST_QEMU[@]}" "${DEP_LIST_NASM[@]:-}")
    # Remove duplicates
    ALL_DEPS=($(echo "${ALL_DEPS[@]}" | tr ' ' '\n' | sort -u))

    for dep in "${ALL_DEPS[@]}"; do
        show_dep_status "$dep"
    done

    # ── Find missing ────────────────────────────────────────────────────
    ALL_DEPS=(gcc binutils make xorriso mtools python qemu nasm)
    MISSING=()
    for dep in "${ALL_DEPS[@]}"; do
        local func="check_${dep}"
        if ! $func 2>/dev/null; then
            MISSING+=("$dep")
        fi
    done

    echo ""
    if [[ ${#MISSING[@]} -eq 0 ]]; then
        echo -e "  ${CHECK}  ${G}${BOLD}All dependencies are installed!${RESET}"
        echo ""
        echo -e "  ${INFO}  You can now build NucleOS:"
        echo -e "      ${C}make clean && make${RESET}"
        echo -e "      ${C}make iso${RESET}"
        echo -e "      ${C}make run${RESET}"
        echo ""
        return 0
    fi

    echo -e "  ${WARN}  ${Y}${BOLD}${#MISSING[@]} package(s) missing:${RESET}"
    for dep in "${MISSING[@]}"; do
        echo -e "      ${R}•${RESET} ${DEP_NAMES[$dep]:-$dep}"
    done

    # ── Ask to install ──────────────────────────────────────────────────
    echo ""
    if [[ -z "$PKG_MGR" ]]; then
        echo -e "  ${FAIL}  ${R}Could not detect a supported package manager.${RESET}"
        echo -e "  ${INFO}  Install manually:"
        echo -e "      ${W}gcc${RESET}  ${W}binutils${RESET}  ${W}make${RESET}  ${W}xorriso${RESET}  ${W}mtools${RESET}  ${W}python3${RESET}  ${W}qemu-system-x86_64${RESET}  ${W}nasm${RESET}"
        echo ""
        return 1
    fi

    echo -e "  ${ASK}  ${W}Install missing packages via ${C}${PKG_MGR}${W}?${RESET}"
    echo -e "  ${DIM}     This will run with sudo if needed.${RESET}"

    if ! ask_confirm "Proceed with installation?" "y"; then
        echo ""
        echo -e "  ${INFO}  Skipped. Install manually later."
        echo ""
        return 0
    fi

    # ── Install ─────────────────────────────────────────────────────────
    section "Installing Packages"

    for dep in "${MISSING[@]}"; do
        local name="${DEP_NAMES[$dep]:-$dep}"
        echo -e "  ${ARROW}  Installing ${W}${name}${RESET}..."
        if do_install "$dep" 2>&1 | while IFS= read -r line; do
            echo -e "     ${DIM}${line}${RESET}"
        done; then
            log_ok "$name"
        else
            log_fail "$name"
        fi
    done

    # ── Post-install verify ─────────────────────────────────────────────
    section "Verification"

    local all_ok=true
    for dep in "${MISSING[@]}"; do
        local func="check_${dep}"
        local name="${DEP_NAMES[$dep]:-$dep}"
        if $func 2>/dev/null; then
            log_ok "$name"
        else
            log_fail "$name  ${R}(still missing)${RESET}"
            all_ok=false
        fi
    done

    echo ""
    if $all_ok; then
        echo -e "  ${CHECK}  ${G}${BOLD}All dependencies installed successfully!${RESET}"
    else
        echo -e "  ${WARN}  ${Y}Some packages failed. Check errors above.${RESET}"
        echo -e "  ${INFO}  Try installing manually or visit:"
        echo -e "      ${C}https://github.com/NucleOS/cris-os-v2${RESET}"
    fi
    echo ""
    echo -e "  ${INFO}  Build NucleOS:"
    echo -e "      ${C}make clean && make${RESET}"
    echo -e "      ${C}make iso${RESET}"
    echo -e "      ${C}make run${RESET}"
    echo ""
}

# ── Unattended mode ─────────────────────────────────────────────────────────
if [[ "${1:-}" == "-y" ]] || [[ "${1:-}" == "--yes" ]]; then
    AUTO_YES=1
fi

if [[ "${1:-}" == "-h" ]] || [[ "${1:-}" == "--help" ]]; then
    echo "Usage: $0 [-y|--yes] [-h|--help]"
    echo ""
    echo "Options:"
    echo "  -y, --yes     Skip prompts, install everything"
    echo "  -h, --help    Show this help"
    echo ""
    echo "Supported platforms:"
    echo "  Arch, Manjaro, EndeavourOS, Garuda"
    echo "  Ubuntu, Debian, Linux Mint, Pop!_OS, Kali, Zorin"
    echo "  Fedora, openSUSE, SLES"
    echo "  Void Linux, Alpine, Gentoo, NixOS"
    echo "  FreeBSD, macOS (Homebrew)"
    echo "  Windows (MSYS2, Git Bash, WSL)"
    exit 0
fi

if [[ "${AUTO_YES:-}" == "1" ]]; then
    # Override ask_confirm to always return true
    ask_confirm() { return 0; }
fi

main "$@"
