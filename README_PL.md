# PolandOS

**Jadro: Orzeł | Wersja: 0.0.1 | Architektura: x86_64**

> Polska nigdy nie zginie. Pisz jądro.

## O systemie

PolandOS to autorski system operacyjny napisany w całości w C17 i asemblerze x86_64 (NASM).
Zainspirowany duchem Linuksa 0.01, ale celujący w nowoczesny sprzęt x86_64 (lata 20. XXI wieku).

Jądro nosi kryptonim **Orzeł** — symbol siły i dumy polskiego narodu.

## Wymagania systemowe

- Procesor: x86_64 z obsługą APIC
- Pamięć RAM: minimum 512 MB
- Grafika: Framebuffer zgodny z Limine
- Bootloader: Limine v6+
- Sieć: Intel e1000 (w QEMU)
- Dysk: NVMe (opcjonalnie)

## Struktura projektu

```
PolandOS/
├── kernel/
│   ├── kmain.c                        # Punkt wejścia jądra
│   ├── arch/x86_64/
│   │   ├── boot/boot.asm              # _start — punkt wejścia asemblera
│   │   ├── cpu/
│   │   │   ├── gdt.{c,h}             # Global Descriptor Table
│   │   │   ├── idt.{c,h}             # Interrupt Descriptor Table
│   │   │   ├── apic.{c,h}            # Local/IO APIC
│   │   │   ├── gdt_flush.asm         # Przeładowanie GDT
│   │   │   └── isr_stubs.asm         # Handlery przerwań (stub)
│   │   └── mm/
│   │       ├── pmm.{c,h}             # Physical Memory Manager
│   │       ├── vmm.{c,h}             # Virtual Memory Manager (4-level paging)
│   │       └── heap.{c,h}            # Alokator sterty jądra (kmalloc/kfree)
│   ├── drivers/
│   │   ├── serial.{c,h}              # UART szeregowy
│   │   ├── fb.{c,h}                  # Framebuffer + konsola tekstowa
│   │   ├── keyboard.{c,h}            # Klawiatura PS/2
│   │   ├── timer.{c,h}               # HPET
│   │   ├── rtc.{c,h}                 # Real Time Clock
│   │   ├── pci.{c,h}                 # PCI/PCIe (ECAM)
│   │   ├── nvme.{c,h}                # NVMe SSD
│   │   ├── e1000.{c,h}               # Intel e1000 karta sieciowa
│   │   └── speaker.{c,h}             # PC speaker / brzęczyk
│   ├── acpi/acpi.{c,h}               # Parsowanie tabel ACPI
│   ├── net/
│   │   ├── ethernet.{c,h}            # Warstwa Ethernet II
│   │   ├── arp.{c,h}                 # ARP
│   │   ├── ipv4.{c,h}                # IPv4
│   │   ├── icmp.{c,h}                # ICMP (ping)
│   │   ├── udp.{c,h}                 # UDP
│   │   ├── tcp.{c,h}                 # TCP (szkielet)
│   │   ├── dhcp.{c,h}                # DHCP klient
│   │   └── dns.{c,h}                 # Resolver DNS
│   ├── shell/shell.{c,h}             # Powłoka interaktywna
│   └── lib/
│       ├── string.{c,h}              # Operacje na łańcuchach
│       ├── printf.{c,h}              # kprintf/ksnprintf
│       └── panic.{c,h}               # kpanic
├── include/
│   ├── types.h                        # Typy podstawowe (u8, u16, ...)
│   ├── limine.h                       # Protokół Limine v6
│   └── io.h                           # inb/outb/inl/outl
├── linker.ld                          # Skrypt linkera (higher-half)
├── limine.cfg                         # Konfiguracja bootloadera
├── Makefile                           # System budowania
├── build.sh                           # Skrypt budowania (Arch Linux)
├── burn.sh                            # Nagrywanie ISO na pendrive (Arch Linux)
├── README_PL.md                       # Ten plik
└── TESTING.md                         # Przewodnik testowania
```

## Budowanie

### Wymagania narzędziowe

- **GCC cross-compiler**: `x86_64-elf-gcc` (bez bibliotek systemowych)
- **NASM**: asembler dla x86_64
- **xorriso**: tworzenie obrazów ISO
- **QEMU**: testowanie (pakiet `qemu-system-x86`)
- **OVMF**: firmware UEFI dla QEMU (`/usr/share/ovmf/OVMF.fd`)

Instalacja na Ubuntu/Debian:
```bash
sudo apt install nasm xorriso qemu-system-x86 ovmf
# Cross-compiler należy zbudować samodzielnie lub pobrać gotowy
```

### Kompilacja

```bash
make all     # kompilacja jądra → polandos.elf
make iso     # tworzenie obrazu ISO → polandos.iso
make run     # uruchomienie w QEMU
make debug   # debugowanie z GDB (QEMU zatrzymuje się na starcie)
make clean   # czyszczenie plików pośrednich
```

### Nagrywanie na pendrive (Arch Linux)

Użyj skryptu `burn.sh`, aby nagrać obraz ISO na pendrive USB:

```bash
sudo ./burn.sh              # Interaktywny — wyświetla listę urządzeń USB
sudo ./burn.sh /dev/sdX     # Nagraj bezpośrednio na /dev/sdX (wymaga potwierdzenia)
```

Skrypt automatycznie buduje ISO, jeśli `polandos.iso` nie istnieje.
**Uwaga:** Wszystkie dane na wybranym pendrive zostaną usunięte!

### Zmienne budowania

Można nadpisać kompilator przez zmienną `CROSS`:
```bash
make CROSS=x86_64-linux-gnu- all
```

## Funkcje jądra

### Rozruch
- Bootloader **Limine v6** w protokole Limine
- Ekran startowy w framebufferze (barwy biało-czerwone)
- Odtwarzanie **Mazurka Dąbrowskiego** przez PC speaker

### Procesor (CPU)
- **GDT**: 64-bitowe segmenty kodu/danych dla ring 0
- **IDT**: Obsługa wyjątków (0–31) i przerwań sprzętowych (32+)
- **APIC**: Local APIC + IO APIC zamiast przestarzałego PIC 8259

### Pamięć
- **PMM**: Physical Memory Manager — alokacja ramek 4 KB za pomocą bitmapy
- **VMM**: Virtual Memory Manager — 4-poziomowe stronicowanie (PML4)
- **Heap**: Alokator sterty jądra `kmalloc`/`kfree` z blokami wolnych miejsc

### Sterowniki
- **Szeregowy (UART)**: wyjście diagnostyczne na COM1
- **Framebuffer**: tryb graficzny, konsola tekstowa z kolorami
- **Klawiatura PS/2**: odczyt klawiszy, obsługa przerwania IRQ1
- **HPET**: High Precision Event Timer — taktowanie jądra (1000 Hz)
- **RTC**: Real Time Clock — data i czas
- **PCI/PCIe**: enumeracja przez ECAM (MCFG z ACPI)
- **NVMe**: sterownik dysków SSD NVMe
- **e1000**: karta sieciowa Intel e1000 (kompatybilna z QEMU)
- **PC Speaker**: brzęczyk, odtwarzanie melodii

### ACPI
- Parsowanie RSDP, RSDT/XSDT, MADT, FADT, HPET, MCFG
- `acpi_power_off()` — wyłączanie przez PM1a CNT
- `acpi_reset()` — reset przez FADT reset register

### Sieć
- **Ethernet II**: wysyłanie i odbieranie ramek
- **ARP**: rozwiązywanie adresów MAC
- **IPv4**: routing, fragmentacja
- **ICMP**: ping (echo request/reply) z pomiarem RTT
- **UDP**: bezpołączeniowy transport
- **TCP**: szkielet stosu TCP
- **DHCP**: klient DORA — automatyczne uzyskiwanie adresu IP
- **DNS**: resolver nazw domenowych (UDP port 53)

## Powłoka systemowa

Po uruchomieniu PolandOS automatycznie uruchamia powłokę z polskim interfejsem.
Dostępne polecenia:

| Polecenie        | Opis                              |
|------------------|-----------------------------------|
| `pomoc`          | Wyświetla pomoc                   |
| `info`           | Informacje o systemie i uptime    |
| `pamiec`         | Statystyki pamięci RAM            |
| `siec`           | Konfiguracja sieci (MAC, IP, ...)  |
| `ping <ip>`      | Wysyła ICMP echo na adres IP      |
| `dns <host>`     | Rozwiązuje nazwę domenową         |
| `pci`            | Lista urządzeń PCI                |
| `czas`           | Aktualna data i czas              |
| `dysk`           | Informacje o dysku NVMe           |
| `wyczysc`        | Czyści ekran                      |
| `wylacz`         | Wyłącza komputer (ACPI)           |
| `restart`        | Restartuje komputer (ACPI)        |
| `panika`         | Wymusza kernel panic (test)       |
| `echo <tekst>`   | Wyświetla podany tekst            |
| `hex <addr> <n>` | Zrzut pamięci w formacie hex      |

Powłoka obsługuje:
- Historię 16 ostatnich poleceń (strzałki ↑↓)
- Kasowanie znaków (Backspace)
- Usuwanie całej linii (Ctrl+U)
- Rotujące polskie żarty programistyczne przy starcie (MOTD)

## CI / GitHub Actions

PolandOS zawiera workflow GitHub Actions, który automatycznie buduje jądro (ELF) oraz bootowalny obraz ISO przy każdym pushu i pull requeście do gałęzi `main`.

### Ręczne uruchamianie budowania

1. Wejdź w zakładkę **Actions** w repozytorium na GitHubie.
2. Wybierz workflow **Build PolandOS** z bocznego panelu.
3. Kliknij **Run workflow** → wybierz gałąź → **Run workflow**.

### Pobieranie artefaktów

Po zakończeniu workflow:

1. Otwórz ukończony workflow run w zakładce **Actions**.
2. Przewiń do sekcji **Artifacts** na dole podsumowania.
3. Pobierz **polandos.iso** (bootowalny ISO) lub **polandos.elf** (samo jądro).

### Uruchamianie pobranego ISO

Uruchom ISO w QEMU (wymagane `qemu-system-x86_64` i OVMF):

```bash
qemu-system-x86_64 \
    -M q35 -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -cdrom polandos.iso \
    -boot d \
    -serial stdio \
    -no-reboot
```

Lub nagraj na pendrive USB i uruchom na prawdziwym sprzęcie — patrz **Nagrywanie na pendrive** powyżej.

## Prawa autorskie

PolandOS © 2024 — Oryginalne dzieło.
Kod napisany od zera. Brak kodu GPL. Brak zewnętrznych bibliotek.

Dla Polski. Dla wolności. Dla kodu.
