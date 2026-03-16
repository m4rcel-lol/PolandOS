#!/usr/bin/env bash
# PolandOS — Build script
# Installs dependencies and builds the kernel / bootable ISO.
#
# Usage:
#   ./build.sh              # install deps + build bootable ISO
#   ./build.sh all          # install deps + build kernel ELF only
#   ./build.sh iso          # install deps + build bootable ISO
#   ./build.sh clean        # clean build artifacts
#   ./build.sh deps         # only install dependencies
#
# Supports Arch Linux / CachyOS (pacman) and Ubuntu / Debian (apt).
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

USE_PACMAN=false
USE_APT=false

if command -v pacman &>/dev/null; then
    USE_PACMAN=true
elif command -v apt-get &>/dev/null; then
    USE_APT=true
else
    error "Neither pacman nor apt-get found."
    error "Install manually: gcc (or gcc-x86_64-linux-gnu), binutils (or binutils-x86_64-linux-gnu), nasm, xorriso, curl, make"
    exit 1
fi

# ─── Required packages ──────────────────────────────────────────────────────

install_deps() {
    if $USE_PACMAN; then
        # base-devel : gcc, make, ld, binutils, etc.
        # nasm       : assembler for x86_64
        # xorriso    : ISO image creation
        # curl       : downloading Limine bootloader
        local packages=(base-devel nasm xorriso curl)
        local needed=()

        for pkg in "${packages[@]}"; do
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
    elif $USE_APT; then
        # gcc-x86-64-linux-gnu      : cross-compiler
        # binutils-x86-64-linux-gnu : cross-linker (x86_64-linux-gnu-ld)
        # nasm                      : assembler for x86_64
        # xorriso                   : ISO image creation
        # curl                      : downloading Limine bootloader
        # make                      : build tool
        local packages=(gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu nasm xorriso curl make)
        local needed=()

        for pkg in "${packages[@]}"; do
            if ! dpkg -s "$pkg" &>/dev/null 2>&1; then
                needed+=("$pkg")
            fi
        done

        if [[ ${#needed[@]} -gt 0 ]]; then
            warn "The following packages will be installed: ${needed[*]}"
            sudo apt-get update -qq
            sudo apt-get install -y "${needed[@]}"
        else
            info "All required packages are already installed."
        fi
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
    all|iso)
        ;;
    *)
        error "Unknown target: $TARGET"
        echo "Usage: $0 [all|iso|clean|deps]"
        exit 1
        ;;
esac

# Install dependencies
install_deps

# On Arch x86_64 the native GCC works — set CROSS to empty string so the
# Makefile uses 'gcc' and 'ld' instead of 'x86_64-linux-gnu-gcc' etc.
# On Debian/Ubuntu the cross-compiler packages are used (default CROSS prefix).
MAKE_ARGS=()
if $USE_PACMAN; then
    MAKE_ARGS=(CROSS=)
fi

# Build
if [[ "$TARGET" == "all" ]]; then
    info "Building kernel ELF..."
    make "${MAKE_ARGS[@]}" all
    info "Done! Kernel ELF: polandos.elf"
else
    info "Building bootable ISO..."
    make "${MAKE_ARGS[@]}" iso
    info "Done! Bootable ISO: polandos.iso"
fi
