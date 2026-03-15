#!/usr/bin/env bash
# PolandOS — Build script for Arch Linux / CachyOS
# Installs dependencies via pacman and builds the kernel + ISO.
#
# Usage:
#   ./build.sh              # install deps + build kernel
#   ./build.sh iso          # install deps + build bootable ISO
#   ./build.sh run          # install deps + build + run in QEMU
#   ./build.sh debug        # install deps + build + run with GDB stub
#   ./build.sh clean        # clean build artifacts
#   ./build.sh deps         # only install dependencies
#
# On Arch x86_64 the native GCC is used (no cross-compiler needed).

set -euo pipefail

# ─── Colors ──────────────────────────────────────────────────────────────────

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[DOBRZE]${NC} $*"; }
warn()  { echo -e "${YELLOW}[UWAGA]${NC} $*"; }
error() { echo -e "${RED}[BLAD]${NC}  $*"; }

# ─── Detect distro ──────────────────────────────────────────────────────────

if ! command -v pacman &>/dev/null; then
    error "pacman not found — this script is for Arch Linux / CachyOS."
    error "On Ubuntu/Debian use: sudo apt install nasm xorriso qemu-system-x86 ovmf"
    exit 1
fi

# ─── Required packages ──────────────────────────────────────────────────────

# base-devel : gcc, make, ld, binutils, etc.
# nasm       : assembler for x86_64
# xorriso    : ISO image creation
# qemu-system-x86 : QEMU emulator for x86_64 (needed for 'run'/'debug')
# edk2-ovmf  : UEFI firmware for QEMU (needed for 'run'/'debug')
# curl       : downloading Limine bootloader

PACKAGES_BUILD=(base-devel nasm xorriso curl)
PACKAGES_RUN=(qemu-system-x86 edk2-ovmf)

install_deps() {
    local target="$1"
    local needed=()

    # Always need build dependencies
    for pkg in "${PACKAGES_BUILD[@]}"; do
        if ! pacman -Qi "$pkg" &>/dev/null; then
            needed+=("$pkg")
        fi
    done

    # For run/debug, also need QEMU + OVMF
    if [[ "$target" == "run" || "$target" == "debug" ]]; then
        for pkg in "${PACKAGES_RUN[@]}"; do
            if ! pacman -Qi "$pkg" &>/dev/null; then
                needed+=("$pkg")
            fi
        done
    fi

    if [[ ${#needed[@]} -gt 0 ]]; then
        warn "The following packages will be installed: ${needed[*]}"
        sudo pacman -S --needed "${needed[@]}"
    else
        info "All required packages are already installed."
    fi
}

# ─── Detect OVMF path ───────────────────────────────────────────────────────

find_ovmf() {
    local candidates=(
        /usr/share/edk2/x64/OVMF.4m.fd
        /usr/share/edk2/x64/OVMF.fd
        /usr/share/edk2-ovmf/x64/OVMF.fd
        /usr/share/OVMF/x64/OVMF.fd
        /usr/share/ovmf/OVMF.fd
        /usr/share/ovmf/x64/OVMF.fd
        /usr/share/qemu/OVMF.fd
    )

    for path in "${candidates[@]}"; do
        if [[ -f "$path" ]]; then
            echo "$path"
            return
        fi
    done

    # Not found — return empty
    echo ""
}

# ─── Main ────────────────────────────────────────────────────────────────────

cd "$(dirname "$0")"

TARGET="${1:-all}"

case "$TARGET" in
    clean)
        info "Cleaning build artifacts..."
        make clean
        exit 0
        ;;
    deps)
        install_deps "run"
        exit 0
        ;;
    all|iso|run|debug)
        ;;
    *)
        error "Unknown target: $TARGET"
        echo "Usage: $0 [all|iso|run|debug|clean|deps]"
        exit 1
        ;;
esac

# Install dependencies
install_deps "$TARGET"

# On Arch x86_64 the native GCC works — set CROSS to empty string so the
# Makefile uses 'gcc' and 'ld' instead of 'x86_64-linux-gnu-gcc' etc.
MAKE_ARGS=(CROSS=)

# For run/debug, find the correct OVMF path
if [[ "$TARGET" == "run" || "$TARGET" == "debug" ]]; then
    OVMF_PATH="$(find_ovmf)"
    if [[ -z "$OVMF_PATH" ]]; then
        error "OVMF firmware not found. Install it with: sudo pacman -S edk2-ovmf"
        exit 1
    fi
    info "Using OVMF: $OVMF_PATH"
    MAKE_ARGS+=(OVMF="$OVMF_PATH")
fi

# Build
info "Building target: $TARGET"
make "${MAKE_ARGS[@]}" "$TARGET"

info "Done!"
