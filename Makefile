# PolandOS — System budowania
# Jadro Orzel

CROSS ?= x86_64-linux-gnu-
CC    := $(CROSS)gcc
AS    := nasm
LD    := $(CROSS)ld
OVMF  ?= /usr/share/ovmf/OVMF.fd

CFLAGS := -std=c17 -ffreestanding -nostdlib -mno-red-zone -mno-sse -mno-sse2 \
          -mcmodel=kernel -fno-pic -fno-pie -fno-stack-protector -O2 -Wall -Wextra \
          -Wno-unused-parameter -Iinclude -Ikernel

ASFLAGS := -f elf64

LDFLAGS := -T linker.ld -nostdlib -static

TARGET := polandos.elf
ISO    := polandos.iso

KERNEL_SRCS := \
    kernel/kmain.c \
    kernel/arch/x86_64/cpu/gdt.c \
    kernel/arch/x86_64/cpu/idt.c \
    kernel/arch/x86_64/cpu/apic.c \
    kernel/arch/x86_64/mm/pmm.c \
    kernel/arch/x86_64/mm/vmm.c \
    kernel/arch/x86_64/mm/heap.c \
    kernel/drivers/serial.c \
    kernel/drivers/fb.c \
    kernel/drivers/keyboard.c \
    kernel/drivers/mouse.c \
    kernel/drivers/timer.c \
    kernel/drivers/rtc.c \
    kernel/drivers/pci.c \
    kernel/drivers/nvme.c \
    kernel/drivers/e1000.c \
    kernel/drivers/speaker.c \
    kernel/drivers/gpu.c \
    kernel/acpi/acpi.c \
    kernel/fs/vfs.c \
    kernel/net/ethernet.c \
    kernel/net/arp.c \
    kernel/net/ipv4.c \
    kernel/net/icmp.c \
    kernel/net/udp.c \
    kernel/net/tcp.c \
    kernel/net/dhcp.c \
    kernel/net/dns.c \
    kernel/gui/window.c \
    kernel/gui/desktop.c \
    kernel/gui/widget.c \
    kernel/shell/shell.c \
    kernel/services/service.c \
    kernel/installer/installer.c \
    kernel/lib/string.c \
    kernel/lib/printf.c \
    kernel/lib/panic.c

ASM_SRCS := \
    kernel/arch/x86_64/boot/boot.asm \
    kernel/arch/x86_64/cpu/isr_stubs.asm \
    kernel/arch/x86_64/cpu/gdt_flush.asm

OBJS := $(KERNEL_SRCS:.c=.o) $(ASM_SRCS:.asm=.o)

# ─── Targets ─────────────────────────────────────────────────────────────────

.PHONY: all iso run debug clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  LD      $@"
	$(LD) $(LDFLAGS) -o $@ $^

# Compile C sources
%.o: %.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM sources
%.o: %.asm
	@echo "  AS      $<"
	$(AS) $(ASFLAGS) $< -o $@

# ─── ISO ─────────────────────────────────────────────────────────────────────

iso: $(TARGET)
	@# Download Limine if needed
	@if [ ! -f limine/limine ]; then \
	    echo "  Pobieranie Limine..."; \
	    mkdir -p limine; \
	    curl -sL https://github.com/limine-bootloader/limine/archive/refs/tags/v7.13.3-binary.tar.gz \
	        | tar -xz --strip-components=1 -C limine || \
	    (echo "  Blad pobierania Limine — zainstaluj recznie do katalogu limine/" && exit 1); \
	    $(MAKE) -C limine || \
	    (echo "  Blad kompilacji Limine" && exit 1); \
	fi
	@# Clean and create directory structure
	rm -rf iso_root
	mkdir -p iso_root/boot/limine
	mkdir -p iso_root/EFI/BOOT
	@# Copy kernel
	cp $(TARGET) iso_root/boot/polandos.elf
	@# Copy Limine config (Limine v7 uses limine.cfg)
	cp limine.cfg iso_root/boot/limine/limine.cfg
	@# Copy Limine BIOS/UEFI files
	cp limine/limine-bios.sys      iso_root/boot/limine/
	cp limine/limine-bios-cd.bin   iso_root/boot/limine/
	cp limine/limine-uefi-cd.bin   iso_root/boot/limine/
	cp limine/BOOTX64.EFI          iso_root/EFI/BOOT/BOOTX64.EFI
	@# Build ISO (Rock Ridge for proper filesystem traversal)
	xorriso -as mkisofs -R \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image \
	    --protective-msdos-label \
	    iso_root -o $(ISO)
	@# Install Limine BIOS boot sector
	limine/limine bios-install $(ISO)
	@echo "  ISO     $(ISO) gotowy"

# ─── QEMU ────────────────────────────────────────────────────────────────────

nvme.img:
	dd if=/dev/zero of=nvme.img bs=1M count=64

run: iso nvme.img
	qemu-system-x86_64 \
	    -M q35 \
	    -m 512M \
	    -bios $(OVMF) \
	    -vga std \
	    -drive file=$(ISO),format=raw,if=ide \
	    -drive file=nvme.img,if=none,id=nvme0 \
	    -device nvme,drive=nvme0,serial=deadbeef \
	    -netdev user,id=net0 \
	    -device e1000,netdev=net0 \
	    -serial stdio \
	    -no-reboot

debug: iso nvme.img
	qemu-system-x86_64 \
	    -M q35 \
	    -m 512M \
	    -bios $(OVMF) \
	    -vga std \
	    -drive file=$(ISO),format=raw,if=ide \
	    -drive file=nvme.img,if=none,id=nvme0 \
	    -device nvme,drive=nvme0,serial=deadbeef \
	    -netdev user,id=net0 \
	    -device e1000,netdev=net0 \
	    -serial stdio \
	    -no-reboot \
	    -s -S

# ─── Clean ───────────────────────────────────────────────────────────────────

clean:
	@echo "  CLEAN"
	rm -f $(OBJS) $(TARGET) $(ISO)
