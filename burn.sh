#!/usr/bin/env bash
# PolandOS — USB burn script for Arch Linux / CachyOS
# Burns polandos.iso onto a USB drive.
#
# Usage:
#   ./burn.sh              # interactive — lists USB devices and prompts
#   ./burn.sh /dev/sdX     # burn directly to /dev/sdX (still asks confirmation)
#
# The script builds the ISO automatically if polandos.iso does not exist.
# WARNING: This WILL erase all data on the selected USB drive!

set -euo pipefail

# ─── Colors ──────────────────────────────────────────────────────────────────

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[DOBRZE]${NC} $*"; }
warn()  { echo -e "${YELLOW}[UWAGA]${NC} $*"; }
error() { echo -e "${RED}[BLAD]${NC}  $*"; }

# ─── Root check ──────────────────────────────────────────────────────────────

if [[ $EUID -ne 0 ]]; then
    error "This script must be run as root (sudo ./burn.sh)"
    exit 1
fi

# ─── Detect distro ──────────────────────────────────────────────────────────

if ! command -v pacman &>/dev/null; then
    error "pacman not found — this script is for Arch Linux / CachyOS."
    exit 1
fi

# ─── Ensure required tools ──────────────────────────────────────────────────

for cmd in dd lsblk; do
    if ! command -v "$cmd" &>/dev/null; then
        error "Required command '$cmd' not found."
        exit 1
    fi
done

# ─── Change to script directory ─────────────────────────────────────────────

cd "$(dirname "$0")"

ISO="polandos.iso"

# ─── Build ISO if missing ───────────────────────────────────────────────────

if [[ ! -f "$ISO" ]]; then
    warn "ISO not found — building $ISO first..."
    # Run the build as the real (non-root) user if invoked via sudo
    if [[ -n "${SUDO_USER:-}" ]]; then
        sudo -u "$SUDO_USER" bash build.sh iso
    else
        bash build.sh iso
    fi

    if [[ ! -f "$ISO" ]]; then
        error "Build failed — $ISO was not created."
        exit 1
    fi
fi

info "ISO found: $ISO ($(du -h "$ISO" | cut -f1))"

# ─── List removable USB devices ─────────────────────────────────────────────

list_usb_devices() {
    lsblk -dnpo NAME,SIZE,MODEL,TRAN | awk '$NF == "usb" { $NF=""; sub(/[[:space:]]+$/, ""); print }'
}

select_device() {
    local devices
    devices=$(list_usb_devices)

    if [[ -z "$devices" ]]; then
        error "No USB devices found. Plug in a USB drive and try again."
        exit 1
    fi

    echo ""
    echo -e "${BOLD}Available USB devices:${NC}"
    echo "────────────────────────────────────────────────"

    local i=1
    local dev_list=()
    while IFS= read -r line; do
        local dev size model
        dev=$(echo "$line" | awk '{print $1}')
        size=$(echo "$line" | awk '{print $2}')
        model=$(echo "$line" | awk '{$1=""; $2=""; print}' | sed 's/^ *//')
        dev_list+=("$dev")
        printf "  ${CYAN}%d)${NC} %-12s %8s  %s\n" "$i" "$dev" "$size" "$model"
        ((i++))
    done <<< "$devices"

    echo "────────────────────────────────────────────────"
    echo ""

    local choice
    while true; do
        read -rp "Select device number [1-$((i-1))]: " choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice < i )); then
            DEVICE="${dev_list[$((choice-1))]}"
            return
        fi
        warn "Invalid choice. Enter a number between 1 and $((i-1))."
    done
}

# ─── Device selection ────────────────────────────────────────────────────────

DEVICE="${1:-}"

if [[ -n "$DEVICE" ]]; then
    # Validate user-supplied device
    if [[ ! -b "$DEVICE" ]]; then
        error "$DEVICE is not a valid block device."
        exit 1
    fi

    # Ensure it is a USB device
    device_tran=$(lsblk -dnpo TRAN "$DEVICE" 2>/dev/null | awk '{print $1}' || true)
    if [[ "$device_tran" != "usb" ]]; then
        warn "$DEVICE does not appear to be a USB device (transport: ${device_tran:-unknown})."
        read -rp "Continue anyway? [y/N]: " yn
        if [[ ! "$yn" =~ ^[Yy]$ ]]; then
            info "Aborted."
            exit 0
        fi
    fi
else
    select_device
fi

# ─── Safety confirmation ────────────────────────────────────────────────────

echo ""
echo -e "${RED}${BOLD}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${RED}${BOLD}║  WARNING: ALL DATA ON ${DEVICE} WILL BE DESTROYED!  ║${NC}"
echo -e "${RED}${BOLD}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
lsblk "$DEVICE"
echo ""

read -rp "Type 'TAK' (yes in Polish) to confirm: " confirm
if [[ "$confirm" != "TAK" ]]; then
    info "Aborted — no changes made."
    exit 0
fi

# ─── Unmount any mounted partitions ─────────────────────────────────────────

info "Unmounting partitions on $DEVICE..."
for part in "${DEVICE}"*; do
    if mountpoint -q "$part" 2>/dev/null || mount | grep -q "^$part "; then
        if umount "$part" 2>/dev/null; then
            info "Unmounted $part"
        fi
    fi
done

# ─── Burn ISO ────────────────────────────────────────────────────────────────

info "Burning $ISO to $DEVICE..."
dd if="$ISO" of="$DEVICE" bs=4M status=progress oflag=sync conv=fsync

# ─── Sync and finish ────────────────────────────────────────────────────────

info "Syncing buffers..."
sync

echo ""
info "PolandOS has been burned to $DEVICE!"
info "You can now boot from this USB drive."
echo -e "${GREEN}${BOLD}Polska nie zginęła! 🇵🇱${NC}"
