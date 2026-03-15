#!/usr/bin/env bash
# PolandOS — Build script for Arch Linux / CachyOS
# Installs dependencies via pacman and builds the bootable ISO.
#
# Usage:
#   ./build.sh              # install deps + build bootable ISO
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
    error "On Ubuntu/Debian use: sudo apt install nasm xorriso curl"
    exit 1
fi

# ─── Required packages ──────────────────────────────────────────────────────

# base-devel : gcc, make, ld, binutils, etc.
# nasm       : assembler for x86_64
# xorriso    : ISO image creation
# curl       : downloading Limine bootloader

PACKAGES_BUILD=(base-devel nasm xorriso curl)

install_deps() {
    local needed=()

    for pkg in "${PACKAGES_BUILD[@]}"; do
        if ! pacman -Qi "$pkg" &>/dev/null; then
            needed+=("$pkg")
        fi
    done

    if [[ ${#needed[@]} -gt 0 ]]; then
        warn "The following packages will be installed: ${needed[*]}"
        sudo pacman -S --needed "${needed[@]}"
    else
        info "All required packages are already installed."
    fi
}

# ─── Main ────────────────────────────────────────────────────────────────────

cd "$(dirname "$0")"

TARGET="${1:-iso}"

case "$TARGET" in
    clean)
        info "Cleaning build artifacts..."
        make clean
        exit 0
        ;;
    deps)
        install_deps
        exit 0
        ;;
    iso)
        ;;
    *)
        error "Unknown target: $TARGET"
        echo "Usage: $0 [iso|clean|deps]"
        exit 1
        ;;
esac

# Install dependencies
install_deps

# On Arch x86_64 the native GCC works — set CROSS to empty string so the
# Makefile uses 'gcc' and 'ld' instead of 'x86_64-linux-gnu-gcc' etc.
MAKE_ARGS=(CROSS=)

# Build the bootable ISO
info "Building bootable ISO..."
make "${MAKE_ARGS[@]}" iso

info "Done! Bootable ISO: polandos.iso"
