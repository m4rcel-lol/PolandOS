# PolandOS — Testing Guide

## Build

```bash
make all   # produces polandos.elf
make iso   # produces polandos.iso (downloads Limine if needed)
```

## Run in QEMU

```bash
make run
```

Expected: UEFI boot → Limine menu (3s timeout) → boot screen (white/red) → Mazurek Dąbrowskiego melody → shell prompt `PolandOS> `.

## Boot Sequence Expected Output (serial)

```
[DOBRZE] PolandOS Orzel kernel booting...
[DOBRZE] Odpowiedzi Limine otrzymane
[DOBRZE] Framebuffer: 1024x768 32bpp
[INFO]   Ekran startowy wyswietlony
[INFO]   Mazurek Dabrowskiego zagrany
[DOBRZE] GDT zainicjalizowany
[DOBRZE] IDT zainicjalizowany
[DOBRZE] PMM: total=512 MB, free=498 MB
[DOBRZE] VMM zainicjalizowany
[DOBRZE] Sterta jadra: 0xFFFF900000000000, 64 MB
[DOBRZE] ACPI zainicjalizowany
[DOBRZE] APIC zainicjalizowany
[DOBRZE] HPET zainicjalizowany
[DOBRZE] Przerwania wlaczone
[DOBRZE] PCI: znaleziono N urzadzen
[DOBRZE] e1000: karta sieciowa zainicjalizowana
[DOBRZE] NVMe: 64 MB pojemnosci
[DOBRZE] RTC: DD.MM.YYYY HH:MM:SS
[DOBRZE] Warstwa sieciowa zainicjalizowana
[DOBRZE] DHCP: adres IP = 10.0.2.15
[DOBRZE] Uruchamianie powloki systemowej...
```

## Shell Commands to Test

```
PolandOS> pomoc
PolandOS> info
PolandOS> pamiec
PolandOS> siec
PolandOS> czas
PolandOS> dysk
PolandOS> pci
PolandOS> echo Witaj Polsko!
PolandOS> hex 0xFFFFFFFF80000000 64
PolandOS> ping 10.0.2.2
PolandOS> dns google.com
PolandOS> wyczysc
PolandOS> panika
```

## Network Testing

QEMU user-mode network (`-netdev user`) provides:
- Gateway: `10.0.2.2`
- DNS: `10.0.2.3`
- DHCP assigned IP: `10.0.2.15`

```
PolandOS> ping 10.0.2.2       # should show RTT
PolandOS> dns google.com      # requires DNS server at 10.0.2.3
PolandOS> siec                # shows MAC, IP, gateway
```

## History Navigation

- Type a command, press Enter
- Press ↑ to recall previous command
- Press ↓ to go forward in history
- Press Ctrl+U to clear current line
- Backspace removes characters

## Debugging with GDB

```bash
make debug   # QEMU starts paused, waiting for GDB on port 1234
```

In another terminal:
```bash
gdb polandos.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
(gdb) layout src
```

## Shutdown / Reboot

```
PolandOS> wylacz    # ACPI power off
PolandOS> restart   # ACPI reset
```

## Known Limitations

- No userspace — ring 0 only
- Single CPU core (no SMP)
- No filesystem — shell only
- TCP stack is skeletal (no connections)
- NVMe read/write available but no FS layer above it
