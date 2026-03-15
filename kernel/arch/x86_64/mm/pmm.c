// PolandOS — Physical Memory Manager (Bitmap PMM)
// Zarządza 4K ramkami fizycznymi za pomocą mapy bitowej
#include "pmm.h"
#include "../../../lib/string.h"
#include "../../../lib/printf.h"
#include "../../../lib/panic.h"

#define PAGE_SIZE 4096ULL
#define BITS_PER_BYTE 8

// ─── Internal memmap entry (matches Limine layout) ───────────────────────────
typedef struct {
    u64 base;
    u64 length;
    u64 type;
} MemmapEntry;

// ─── PMM state ───────────────────────────────────────────────────────────────
u64 hhdm_offset = 0;

static u8  *bitmap     = (u8 *)0;
static u64  bitmap_frames = 0;   // total number of frames tracked
static u64  total_mem  = 0;
static u64  used_mem   = 0;
static u64  last_free  = 0;      // hint: last known free frame index

// ─── Bitmap helpers ──────────────────────────────────────────────────────────
static inline void bitmap_set(u64 frame) {
    bitmap[frame / BITS_PER_BYTE] |= (u8)(1 << (frame % BITS_PER_BYTE));
}

static inline void bitmap_clear(u64 frame) {
    bitmap[frame / BITS_PER_BYTE] &= (u8)~(1 << (frame % BITS_PER_BYTE));
}

static inline int bitmap_test(u64 frame) {
    return (bitmap[frame / BITS_PER_BYTE] >> (frame % BITS_PER_BYTE)) & 1;
}

// ─── pmm_init ────────────────────────────────────────────────────────────────
void pmm_init(void *memmap_entries, u64 count, u64 hhdm_off) {
    hhdm_offset = hhdm_off;
    MemmapEntry *entries = (MemmapEntry *)memmap_entries;

    // Find the highest usable address to determine bitmap size
    u64 highest_addr = 0;
    for (u64 i = 0; i < count; i++) {
        if (entries[i].type == 0) { // USABLE
            u64 end = entries[i].base + entries[i].length;
            if (end > highest_addr) highest_addr = end;
        }
    }

    bitmap_frames = (highest_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 bitmap_size = (bitmap_frames + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    // Round up bitmap_size to page boundary
    bitmap_size = (bitmap_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Find a usable region large enough to hold the bitmap
    bitmap = (u8 *)0;
    for (u64 i = 0; i < count; i++) {
        if (entries[i].type == 0 && entries[i].length >= bitmap_size) {
            bitmap = (u8 *)(entries[i].base + hhdm_offset);
            break;
        }
    }

    if (!bitmap) {
        kpanic("PMM: Nie znaleziono miejsca na bitmap!\n");
    }

    // Mark all frames as used initially
    memset(bitmap, 0xFF, bitmap_size);

    // Accumulate total usable memory and free usable pages
    for (u64 i = 0; i < count; i++) {
        if (entries[i].type == 0) { // USABLE
            total_mem += entries[i].length;
            u64 frame_start = entries[i].base / PAGE_SIZE;
            u64 frame_count = entries[i].length / PAGE_SIZE;
            for (u64 f = frame_start; f < frame_start + frame_count; f++) {
                if (f < bitmap_frames) {
                    bitmap_clear(f);
                }
            }
        }
    }

    // Mark bitmap pages as used (they are in a usable region)
    u64 bitmap_phys  = (u64)bitmap - hhdm_offset;
    u64 bitmap_pages = bitmap_size / PAGE_SIZE;
    for (u64 f = 0; f < bitmap_pages; f++) {
        u64 frame = (bitmap_phys / PAGE_SIZE) + f;
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            used_mem += PAGE_SIZE;
        }
    }

    // used_mem = total_mem minus free pages
    // Recalculate: count free frames
    u64 free_count = 0;
    for (u64 f = 0; f < bitmap_frames; f++) {
        if (!bitmap_test(f)) free_count++;
    }
    used_mem = total_mem - (free_count * PAGE_SIZE);

    kprintf("[PMM] Zainicjalizowany: total=%lluMB used=%lluKB free=%lluMB\n",
            total_mem / (1024*1024),
            used_mem  / 1024,
            (total_mem - used_mem) / (1024*1024));
}

// ─── pmm_alloc ───────────────────────────────────────────────────────────────
u64 pmm_alloc(void) {
    for (u64 i = last_free; i < bitmap_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_mem += PAGE_SIZE;
            last_free = i + 1;
            u64 phys = i * PAGE_SIZE;
            // Zero the page via HHDM
            memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE);
            return phys;
        }
    }
    // Wrap around
    for (u64 i = 0; i < last_free; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_mem += PAGE_SIZE;
            last_free = i + 1;
            u64 phys = i * PAGE_SIZE;
            memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE);
            return phys;
        }
    }
    return 0; // Out of memory
}

// ─── pmm_free ────────────────────────────────────────────────────────────────
void pmm_free(u64 phys) {
    u64 frame = phys / PAGE_SIZE;
    if (frame >= bitmap_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_mem -= PAGE_SIZE;
        if (frame < last_free) last_free = frame;
    }
}

// ─── pmm_alloc_pages ─────────────────────────────────────────────────────────
u64 pmm_alloc_pages(u64 count) {
    if (count == 0) return 0;
    for (u64 i = 0; i + count <= bitmap_frames; i++) {
        int ok = 1;
        for (u64 j = 0; j < count; j++) {
            if (bitmap_test(i + j)) { ok = 0; break; }
        }
        if (ok) {
            for (u64 j = 0; j < count; j++) {
                bitmap_set(i + j);
                used_mem += PAGE_SIZE;
            }
            u64 phys = i * PAGE_SIZE;
            memset((void *)(phys + hhdm_offset), 0, count * PAGE_SIZE);
            return phys;
        }
    }
    return 0;
}

// ─── Stats ───────────────────────────────────────────────────────────────────
u64 pmm_total_bytes(void) { return total_mem; }
u64 pmm_used_bytes(void)  { return used_mem;  }
u64 pmm_free_bytes(void)  { return total_mem > used_mem ? total_mem - used_mem : 0; }
