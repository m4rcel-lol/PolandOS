// PolandOS — Instalator systemu na dysku
// Jadro Orzel — zapis systemu na dysk NVMe
//
// Instalator tworzy prosta tablice partycji GPT na dysku NVMe,
// a nastepnie kopiuje obraz jadra do partycji systemowej.

#include "installer.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../drivers/fb.h"
#include "../drivers/keyboard.h"
#include "../drivers/nvme.h"
#include "../drivers/timer.h"
#include "../../include/types.h"

// ─── Kolory ──────────────────────────────────────────────────────────────────

#define COLOR_GREEN   0x00CC00
#define COLOR_RED     0xFF3333
#define COLOR_YELLOW  0xFFFF00
#define COLOR_CYAN    0x00CCCC
#define COLOR_WHITE   0xFFFFFF
#define COLOR_BLACK   0x000000

// ─── GPT structures ─────────────────────────────────────────────────────────

#define GPT_SIGNATURE  0x5452415020494645ULL  // "EFI PART"
#define LBA_SIZE       512

// Protective MBR (LBA 0)
typedef struct __attribute__((packed)) {
    u8  bootstrap[446];
    struct {
        u8  status;
        u8  first_chs[3];
        u8  type;
        u8  last_chs[3];
        u32 first_lba;
        u32 sectors;
    } partitions[4];
    u16 signature;  // 0xAA55
} MBR;

// GPT Header (LBA 1)
typedef struct __attribute__((packed)) {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 header_crc32;
    u32 reserved;
    u64 my_lba;
    u64 alt_lba;
    u64 first_usable;
    u64 last_usable;
    u8  disk_guid[16];
    u64 part_entry_lba;
    u32 num_parts;
    u32 part_entry_size;
    u32 part_crc32;
} GPTHeader;

// GPT Partition Entry (128 bytes)
typedef struct __attribute__((packed)) {
    u8  type_guid[16];
    u8  unique_guid[16];
    u64 first_lba;
    u64 last_lba;
    u64 attributes;
    u16 name[36];       // UTF-16LE name
} GPTEntry;

// ─── Stale ───────────────────────────────────────────────────────────────────

// PolandOS system partition type GUID (custom):
// {504F4C41-4E44-4F53-5359-535445504152}
static const u8 polandos_part_type[16] = {
    0x41, 0x4C, 0x4F, 0x50,  // "POLA" (little-endian u32)
    0x44, 0x4E,               // "ND"   (little-endian u16)
    0x53, 0x4F,               // "OS"   (little-endian u16)
    0x53, 0x59,               // "SY"
    0x53, 0x54, 0x45, 0x50, 0x41, 0x52  // "STEPAR"
};

// Simple unique GUID for installation (based on kernel ticks)
static void generate_guid(u8 *guid)
{
    extern volatile u64 kernel_ticks;
    u64 a = kernel_ticks;
    u64 b = kernel_ticks ^ 0xDEADBEEFCAFE1234ULL;
    memcpy(guid, &a, 8);
    memcpy(guid + 8, &b, 8);
    // Set version 4 and variant
    guid[6] = (u8)((guid[6] & 0x0F) | 0x40);
    guid[8] = (u8)((guid[8] & 0x3F) | 0x80);
}

// Simple CRC32 (for GPT headers)
static u32 crc32(const u8 *data, size_t len)
{
    u32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void print_ok(const char *msg)
{
    fb_set_color(COLOR_GREEN, COLOR_BLACK);
    fb_puts("  [OK] ");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts(msg);
    fb_putchar('\n');
}

static void print_err(const char *msg)
{
    fb_set_color(COLOR_RED, COLOR_BLACK);
    fb_puts("  [BLAD] ");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts(msg);
    fb_putchar('\n');
}

static void print_progress(const char *msg)
{
    fb_set_color(COLOR_YELLOW, COLOR_BLACK);
    fb_puts("  [....] ");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts(msg);
    fb_putchar('\n');
}

// Set UTF-16LE name in GPT entry (handles potentially unaligned memory)
static void set_part_name(GPTEntry *entry, const char *src, int max)
{
    for (int i = 0; i < max - 1 && src[i]; i++) {
        u16 ch = (u16)(u8)src[i];
        memcpy(&entry->name[i], &ch, sizeof(u16));
    }
}

// ─── MAGIC MARKER ────────────────────────────────────────────────────────────
// We write a simple superblock at the start of the partition to identify
// a PolandOS installation.

#define POLANDOS_MAGIC      0x504F4C414E444F53ULL  // "POLANDOS"
#define POLANDOS_VERSION    1

typedef struct __attribute__((packed)) {
    u64 magic;
    u32 version;
    u32 kernel_lba_start;   // LBA within the partition where kernel starts
    u32 kernel_sectors;     // number of 512-byte sectors
    u32 flags;
    u8  reserved[488];      // pad to 512 bytes total
} PolandOSSuperblock;

// ─── Instalacja ──────────────────────────────────────────────────────────────

void installer_run(void)
{
    u8 buf[512];

    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("\n ============================================\n");
    fb_puts(" =     Instalator PolandOS — Jadro Orzel    =\n");
    fb_puts(" ============================================\n\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);

    // ── Check NVMe availability ──────────────────────────────────────────────
    u64 total_blocks = nvme_get_block_count(1);
    u32 blk_size     = nvme_get_block_size(1);

    if (!total_blocks || !blk_size) {
        print_err("Brak dysku NVMe — instalacja niemozliwa.");
        fb_puts("\n  Podlacz dysk NVMe i sprobuj ponownie.\n\n");
        return;
    }

    u64 total_mb = (total_blocks * blk_size) / (1024ULL * 1024ULL);
    kprintf("  Wykryto dysk NVMe: %llu MB (%llu blokow x %u B)\n\n",
            total_mb, total_blocks, blk_size);

    // ── Confirmation ─────────────────────────────────────────────────────────
    fb_set_color(COLOR_RED, COLOR_BLACK);
    fb_puts("  UWAGA: Wszystkie dane na dysku zostana usniete!\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts("  Czy chcesz kontynuowac? (t/n): ");

    char c = kb_getchar();
    fb_putchar(c);
    fb_putchar('\n');

    if (c != 't' && c != 'T') {
        fb_puts("\n  Instalacja anulowana.\n\n");
        return;
    }

    fb_putchar('\n');

    // ── Step 1: Write protective MBR (LBA 0) ────────────────────────────────
    print_progress("Tworzenie tablicy partycji MBR...");
    memset(buf, 0, 512);
    MBR *mbr = (MBR *)buf;
    mbr->partitions[0].status    = 0x00;
    mbr->partitions[0].type      = 0xEE;   // GPT protective
    mbr->partitions[0].first_lba = 1;
    u64 max_lba = total_blocks - 1;
    mbr->partitions[0].sectors   = (u32)(max_lba > 0xFFFFFFFF ? 0xFFFFFFFF : max_lba);
    mbr->signature = 0xAA55;

    if (nvme_write_blocks(1, 0, 1, buf) != 0) {
        print_err("Blad zapisu MBR!");
        return;
    }
    print_ok("Tablica MBR zapisana");

    // ── Step 2: Write GPT Header (LBA 1) ────────────────────────────────────
    print_progress("Tworzenie naglowka GPT...");

    // First, prepare partition entries (LBA 2-33)
    // We create one partition spanning most of the disk
    u64 part_start = 2048;  // standard alignment
    u64 part_end   = total_blocks - 2048;
    if (part_end <= part_start) {
        part_end = total_blocks - 34;
    }

    // Write partition entries at LBA 2 (128 entries x 128 bytes = 32 sectors)
    // Only first entry is populated
    u8 part_buf[512];
    for (u32 s = 0; s < 32; s++) {
        memset(part_buf, 0, 512);

        if (s == 0) {
            // First partition entry in first sector
            GPTEntry *entry = (GPTEntry *)part_buf;
            memcpy(entry->type_guid, polandos_part_type, 16);
            generate_guid(entry->unique_guid);
            entry->first_lba  = part_start;
            entry->last_lba   = part_end;
            entry->attributes = 0;
            set_part_name(entry, "PolandOS System", 36);

            // Second entry (empty) fills rest of sector
        }

        if (nvme_write_blocks(1, 2 + s, 1, part_buf) != 0) {
            print_err("Blad zapisu wpisow partycji GPT!");
            return;
        }
    }

    // Calculate partition entry CRC
    // We need to read back all 32 sectors and CRC them
    u32 part_crc = 0xFFFFFFFF;
    for (u32 s = 0; s < 32; s++) {
        if (nvme_read_blocks(1, 2 + s, 1, part_buf) != 0) {
            print_err("Blad odczytu wpisow partycji!");
            return;
        }
        for (int b = 0; b < 512; b++) {
            part_crc ^= part_buf[b];
            for (int j = 0; j < 8; j++) {
                if (part_crc & 1)
                    part_crc = (part_crc >> 1) ^ 0xEDB88320;
                else
                    part_crc >>= 1;
            }
        }
    }
    part_crc = ~part_crc;

    // Write primary GPT header (LBA 1)
    memset(buf, 0, 512);
    GPTHeader *gpt = (GPTHeader *)buf;
    gpt->signature       = GPT_SIGNATURE;
    gpt->revision        = 0x00010000;    // GPT 1.0
    gpt->header_size     = 92;
    gpt->header_crc32    = 0;             // filled below
    gpt->my_lba          = 1;
    gpt->alt_lba         = total_blocks - 1;
    gpt->first_usable    = 34;
    gpt->last_usable     = total_blocks - 34;
    generate_guid(gpt->disk_guid);
    gpt->part_entry_lba  = 2;
    gpt->num_parts       = 128;
    gpt->part_entry_size = 128;
    gpt->part_crc32      = part_crc;

    // Calculate header CRC32
    gpt->header_crc32 = 0;
    gpt->header_crc32 = crc32(buf, 92);

    if (nvme_write_blocks(1, 1, 1, buf) != 0) {
        print_err("Blad zapisu naglowka GPT!");
        return;
    }
    print_ok("Naglowek GPT zapisany");

    // ── Step 3: Write PolandOS superblock at partition start ──────────────────
    print_progress("Zapisywanie superbloku PolandOS...");

    memset(buf, 0, 512);
    PolandOSSuperblock *sb = (PolandOSSuperblock *)buf;
    sb->magic            = POLANDOS_MAGIC;
    sb->version          = POLANDOS_VERSION;
    sb->kernel_lba_start = 8;     // kernel starts 8 sectors into partition
    sb->kernel_sectors   = 0;     // filled after kernel write
    sb->flags            = 0;

    if (nvme_write_blocks(1, part_start, 1, buf) != 0) {
        print_err("Blad zapisu superbloku!");
        return;
    }
    print_ok("Superblok PolandOS zapisany");

    // ── Step 4: Write kernel image ───────────────────────────────────────────
    print_progress("Kopiowanie jadra na dysk...");

    // The kernel is loaded in memory by Limine. We can find its location
    // from the linker symbols defined in linker.ld.
    extern char __kernel_start[];
    extern char __kernel_end[];
    u64 kernel_start_addr = (u64)(uintptr_t)__kernel_start;
    u64 kernel_end_addr   = (u64)(uintptr_t)__kernel_end;
    u64 kernel_size       = kernel_end_addr - kernel_start_addr;

    u32 kernel_sectors = (u32)((kernel_size + 511) / 512);
    u64 kernel_dest_lba = part_start + 8;  // offset within partition

    kprintf("  Rozmiar jadra: %llu bajtow (%u sektorow)\n",
            kernel_size, kernel_sectors);

    // Write kernel sector by sector
    u8 sector_buf[512];
    const u8 *kptr = (const u8 *)(uintptr_t)kernel_start_addr;

    for (u32 s = 0; s < kernel_sectors; s++) {
        u64 offset = (u64)s * 512;
        u64 remain = kernel_size - offset;
        u32 chunk  = (remain > 512) ? 512 : (u32)remain;

        memset(sector_buf, 0, 512);
        memcpy(sector_buf, kptr + offset, chunk);

        if (nvme_write_blocks(1, kernel_dest_lba + s, 1, sector_buf) != 0) {
            print_err("Blad zapisu jadra na dysk!");
            return;
        }

        // Progress every 64 sectors
        if ((s & 63) == 0 || s + 1 == kernel_sectors) {
            kprintf("\r  Postep: %u / %u sektorow", s + 1, kernel_sectors);
        }
    }
    fb_putchar('\n');

    // Update superblock with kernel size
    if (nvme_read_blocks(1, part_start, 1, buf) == 0) {
        sb = (PolandOSSuperblock *)buf;
        sb->kernel_sectors = kernel_sectors;
        nvme_write_blocks(1, part_start, 1, buf);
    }

    print_ok("Jadro skopiowane na dysk");

    // ── Step 5: Verify installation ──────────────────────────────────────────
    print_progress("Weryfikacja instalacji...");

    if (nvme_read_blocks(1, part_start, 1, buf) != 0) {
        print_err("Blad odczytu weryfikacyjnego!");
        return;
    }

    sb = (PolandOSSuperblock *)buf;
    if (sb->magic != POLANDOS_MAGIC) {
        print_err("Weryfikacja nieudana — zly magic!");
        return;
    }

    print_ok("Weryfikacja zakonczona pomyslnie");

    // ── Done ─────────────────────────────────────────────────────────────────
    fb_putchar('\n');
    fb_set_color(COLOR_GREEN, COLOR_BLACK);
    fb_puts(" ============================================\n");
    fb_puts(" =    Instalacja zakonczona pomyslnie!      =\n");
    fb_puts(" =    PolandOS zostal zainstalowany na      =\n");
    fb_puts(" =    dysku NVMe.                           =\n");
    fb_puts(" ============================================\n\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);

    fb_puts("  Mozesz teraz zrestartowac system poleceniem 'restart'.\n\n");
}
