# PolandOS

**Kernel: Orzeł | Version: 0.0.1 | Architecture: x86_64**

> Polska nigdy nie zginie — Poland is not yet lost. Write a kernel.

PolandOS is an original operating system kernel written from scratch in C17 and x86_64 assembly (NASM).
Inspired by the spirit of early Linux, but targeting modern x86_64 hardware.

The kernel is codenamed **Orzeł** (Eagle) — a symbol of strength and Polish national pride.

## Features

### Boot
- **Limine v7** bootloader with the Limine protocol
- White-and-red boot splash screen on the framebuffer
- Plays **Mazurek Dąbrowskiego** (Polish national anthem) via the PC speaker on startup

### CPU
- **GDT** — 64-bit code/data segments for ring 0
- **IDT** — Exception handlers (0–31) and hardware interrupts (32+)
- **APIC** — Local APIC + IO APIC (replaces legacy PIC 8259)

### Memory
- **PMM** — Physical Memory Manager with a 4 KB page-frame bitmap allocator
- **VMM** — Virtual Memory Manager with 4-level paging (PML4)
- **Heap** — Kernel heap allocator (`kmalloc`/`kfree`) with free-block coalescing

### Drivers
| Driver | Description |
|--------|-------------|
| Serial (UART) | Diagnostic output on COM1 |
| Framebuffer | Graphics-mode text console with colours |
| PS/2 Keyboard | IRQ1-driven key input with scancode set 1 |
| HPET | High Precision Event Timer — 1 kHz kernel tick |
| RTC | Real-Time Clock — date and time |
| PCI/PCIe | Device enumeration via ECAM (MCFG from ACPI) |
| NVMe | NVMe SSD storage driver |
| e1000 | Intel e1000 network interface (QEMU-compatible) |
| GPU | Basic GPU detection via PCI |
| PC Speaker | Beep melodies through the system speaker |

### ACPI
- RSDP, RSDT/XSDT, MADT, FADT, HPET, MCFG table parsing
- `acpi_power_off()` — shutdown via PM1a control
- `acpi_reset()` — reboot via FADT reset register

### Networking
- **Ethernet II** — frame send/receive
- **ARP** — MAC address resolution
- **IPv4** — routing and fragmentation
- **ICMP** — ping (echo request/reply) with RTT measurement
- **UDP** — connectionless transport
- **TCP** — skeletal TCP stack
- **DHCP** — DORA client for automatic IP configuration
- **DNS** — domain name resolver (UDP port 53)

### Shell
Interactive command-line shell with a Polish interface:

| Command | Description |
|---------|-------------|
| `pomoc` | Show help |
| `info` | System info and uptime |
| `pamiec` | RAM statistics |
| `siec` | Network config (MAC, IP, …) |
| `ping <ip>` | Send ICMP echo to an IP address |
| `dns <host>` | Resolve a domain name |
| `pci` | List PCI devices |
| `czas` | Current date and time |
| `dysk` | NVMe disk info |
| `wyczysc` | Clear screen |
| `wylacz` | Power off (ACPI) |
| `restart` | Reboot (ACPI) |
| `panika` | Force a kernel panic (test) |
| `echo <text>` | Print text |
| `hex <addr> <n>` | Hex dump memory |

The shell supports command history (↑/↓ arrows), backspace editing, and Ctrl+U to clear the line.

## Project Structure

```
PolandOS/
├── kernel/
│   ├── kmain.c                        # Kernel entry point
│   ├── arch/x86_64/
│   │   ├── boot/boot.asm              # _start — assembly entry
│   │   ├── cpu/
│   │   │   ├── gdt.{c,h}             # Global Descriptor Table
│   │   │   ├── idt.{c,h}             # Interrupt Descriptor Table
│   │   │   ├── apic.{c,h}            # Local/IO APIC
│   │   │   ├── gdt_flush.asm         # GDT reload stub
│   │   │   └── isr_stubs.asm         # Interrupt handler stubs
│   │   └── mm/
│   │       ├── pmm.{c,h}             # Physical Memory Manager
│   │       ├── vmm.{c,h}             # Virtual Memory Manager
│   │       └── heap.{c,h}            # Kernel heap (kmalloc/kfree)
│   ├── drivers/
│   │   ├── serial.{c,h}              # UART serial
│   │   ├── fb.{c,h}                  # Framebuffer + text console
│   │   ├── keyboard.{c,h}            # PS/2 keyboard
│   │   ├── timer.{c,h}               # HPET timer
│   │   ├── rtc.{c,h}                 # Real-Time Clock
│   │   ├── pci.{c,h}                 # PCI/PCIe (ECAM)
│   │   ├── nvme.{c,h}                # NVMe SSD
│   │   ├── e1000.{c,h}               # Intel e1000 NIC
│   │   ├── gpu.{c,h}                 # GPU detection
│   │   └── speaker.{c,h}             # PC speaker / buzzer
│   ├── acpi/acpi.{c,h}               # ACPI table parsing
│   ├── net/
│   │   ├── ethernet.{c,h}            # Ethernet II layer
│   │   ├── arp.{c,h}                 # ARP
│   │   ├── ipv4.{c,h}                # IPv4
│   │   ├── icmp.{c,h}                # ICMP (ping)
│   │   ├── udp.{c,h}                 # UDP
│   │   ├── tcp.{c,h}                 # TCP (skeleton)
│   │   ├── dhcp.{c,h}                # DHCP client
│   │   └── dns.{c,h}                 # DNS resolver
│   ├── shell/shell.{c,h}             # Interactive shell
│   └── lib/
│       ├── string.{c,h}              # String operations
│       ├── printf.{c,h}              # kprintf / ksnprintf
│       └── panic.{c,h}               # kpanic
├── include/
│   ├── types.h                        # Basic types (u8, u16, …)
│   ├── limine.h                       # Limine bootloader protocol
│   └── io.h                           # inb / outb / inl / outl
├── linker.ld                          # Linker script (higher-half)
├── limine.conf                        # Bootloader configuration
├── Makefile                           # Build system
├── build.sh                           # Arch Linux build helper
├── README.md                          # This file
├── README_PL.md                       # Polish documentation
└── TESTING.md                         # Testing guide
```

## System Requirements

- **CPU:** x86_64 with APIC support
- **RAM:** 512 MB minimum
- **Graphics:** Limine-compatible framebuffer
- **Bootloader:** Limine v7+
- **Network:** Intel e1000 (in QEMU)
- **Storage:** NVMe (optional)

## Building

### Prerequisites

- **GCC cross-compiler** — `x86_64-linux-gnu-gcc` (or native GCC on x86_64 Arch Linux)
- **NASM** — x86_64 assembler
- **xorriso** — ISO image creation (only needed for `make iso`)
- **QEMU** — `qemu-system-x86` for testing (only needed for `make run`)
- **OVMF** — UEFI firmware for QEMU (only needed for `make run`)

**Ubuntu / Debian:**
```bash
sudo apt install gcc-x86-64-linux-gnu nasm xorriso qemu-system-x86 ovmf
```

**Arch Linux / CachyOS:**
```bash
# Use the included build.sh helper which installs deps via pacman:
./build.sh all
```

### Compile

```bash
make all          # Build kernel → polandos.elf
make iso          # Build bootable ISO → polandos.iso (downloads Limine if needed)
make run          # Build + run in QEMU
make debug        # Build + run with GDB stub (-s -S)
make clean        # Remove build artefacts
```

You can override the cross-compiler prefix:
```bash
make CROSS=x86_64-linux-gnu- all    # explicit cross prefix
make CROSS= all                     # native compiler on x86_64 host
```

## Known Limitations

- No userspace — runs entirely in ring 0
- Single CPU core (no SMP)
- No filesystem — shell-only interface
- TCP stack is skeletal (no established connections)
- NVMe read/write available but no filesystem layer above it

## License

PolandOS © 2024 — Original work. Written from scratch. No GPL code. No external libraries.

For Poland. For freedom. For code.
