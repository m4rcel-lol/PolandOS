// PolandOS — Alokator sterty jądra (free-list z koalescencją)
// 16-bajtowe wyrównanie, detekcja korupcji przez magiczną liczbę
#include "heap.h"
#include "../../../lib/string.h"
#include "../../../lib/panic.h"
#include "../../../lib/printf.h"

#define HEAP_MAGIC  0xDEADBEEFUL
#define ALIGN       16ULL
#define ALIGN_MASK  (ALIGN - 1ULL)

// ─── Block header ─────────────────────────────────────────────────────────────
typedef struct BlockHeader {
    u32 magic;
    u32 used;           // 1 = allocated, 0 = free
    u64 size;           // usable size (not including header)
    struct BlockHeader *prev;
    struct BlockHeader *next;
} BlockHeader;

#define HEADER_SIZE ((u64)sizeof(BlockHeader))

// Align header size itself to ALIGN
#define HEADER_ALIGNED ((HEADER_SIZE + ALIGN_MASK) & ~ALIGN_MASK)

static BlockHeader *heap_head = (BlockHeader *)0;

// ─── heap_init ───────────────────────────────────────────────────────────────
void heap_init(u64 heap_start, u64 heap_size) {
    // Align start address
    u64 aligned_start = (heap_start + ALIGN_MASK) & ~ALIGN_MASK;
    u64 wasted = aligned_start - heap_start;
    if (heap_size <= wasted + HEADER_ALIGNED) {
        kpanic("heap_init: sterta zbyt mala!\n");
    }

    heap_head = (BlockHeader *)aligned_start;
    heap_head->magic = HEAP_MAGIC;
    heap_head->used  = 0;
    heap_head->size  = heap_size - wasted - HEADER_ALIGNED;
    heap_head->prev  = (BlockHeader *)0;
    heap_head->next  = (BlockHeader *)0;
}

// ─── Internal: split block if it's much larger than needed ───────────────────
static void maybe_split(BlockHeader *blk, u64 size) {
    u64 min_split = HEADER_ALIGNED + ALIGN;
    if (blk->size >= size + min_split) {
        u64 new_off = HEADER_ALIGNED + size;
        BlockHeader *next_blk = (BlockHeader *)((u8 *)blk + new_off);
        next_blk->magic = HEAP_MAGIC;
        next_blk->used  = 0;
        next_blk->size  = blk->size - new_off;
        next_blk->prev  = blk;
        next_blk->next  = blk->next;

        if (blk->next) blk->next->prev = next_blk;
        blk->next = next_blk;
        blk->size = size;
    }
}

// ─── kmalloc ─────────────────────────────────────────────────────────────────
void *kmalloc(size_t size) {
    if (!size || !heap_head) return (void *)0;

    // Round up size to alignment
    u64 aligned = (u64)(size + ALIGN_MASK) & ~ALIGN_MASK;

    BlockHeader *cur = heap_head;
    while (cur) {
        if (cur->magic != HEAP_MAGIC)
            kpanic("kmalloc: korupcja sterty! blok=%p magic=0x%x\n",
                   (void *)cur, cur->magic);
        if (!cur->used && cur->size >= aligned) {
            maybe_split(cur, aligned);
            cur->used = 1;
            // Return pointer to usable data (just after header)
            return (void *)((u8 *)cur + HEADER_ALIGNED);
        }
        cur = cur->next;
    }
    kprintf("[HEAP] kmalloc: brak pamieci! rozmiar=%zu\n", size);
    return (void *)0;
}

// ─── kcalloc ─────────────────────────────────────────────────────────────────
void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

// ─── kfree ───────────────────────────────────────────────────────────────────
void kfree(void *ptr) {
    if (!ptr) return;

    BlockHeader *blk = (BlockHeader *)((u8 *)ptr - HEADER_ALIGNED);
    if (blk->magic != HEAP_MAGIC)
        kpanic("kfree: korupcja sterty! ptr=%p\n", ptr);
    if (!blk->used)
        kpanic("kfree: podwojne zwolnienie! ptr=%p\n", ptr);

    blk->used = 0;

    // Coalesce with next block if it's free
    if (blk->next && !blk->next->used) {
        BlockHeader *nxt = blk->next;
        if (nxt->magic != HEAP_MAGIC)
            kpanic("kfree: korupcja nastepnego bloku! blok=%p\n", (void *)nxt);
        blk->size += HEADER_ALIGNED + nxt->size;
        blk->next  = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
        nxt->magic = 0; // invalidate merged block
    }

    // Coalesce with previous block if it's free
    if (blk->prev && !blk->prev->used) {
        BlockHeader *prv = blk->prev;
        if (prv->magic != HEAP_MAGIC)
            kpanic("kfree: korupcja poprzedniego bloku! blok=%p\n", (void *)prv);
        prv->size += HEADER_ALIGNED + blk->size;
        prv->next  = blk->next;
        if (blk->next) blk->next->prev = prv;
        blk->magic = 0; // invalidate merged block
    }
}

// ─── krealloc ────────────────────────────────────────────────────────────────
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr)     return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return (void *)0; }

    BlockHeader *blk = (BlockHeader *)((u8 *)ptr - HEADER_ALIGNED);
    if (blk->magic != HEAP_MAGIC)
        kpanic("krealloc: korupcja sterty! ptr=%p\n", ptr);

    u64 aligned = (u64)(new_size + ALIGN_MASK) & ~ALIGN_MASK;
    if (aligned <= blk->size) {
        // Fits in existing block (optionally shrink)
        maybe_split(blk, aligned);
        return ptr;
    }

    // Need a larger block
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return (void *)0;
    memcpy(new_ptr, ptr, (size_t)blk->size);
    kfree(ptr);
    return new_ptr;
}
