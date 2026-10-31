#include "kernel.h"

/* ---- physical memory manager: one bit per 4 KiB page (1 = in use) ---- */

static uint64_t hhdm;
static uint8_t *bitmap;
static uint64_t page_count;
static uint64_t free_count;
static uint64_t total_usable;
static uint64_t search_hint;

uint64_t hhdm_phys_to_virt(uint64_t phys) {
    return phys + hhdm;
}

static bool page_used(uint64_t page) {
    return bitmap[page / 8] & (1u << (page % 8));
}

static void mark_used(uint64_t page) {
    if (!page_used(page)) {
        bitmap[page / 8] |= (1u << (page % 8));
        free_count--;
    }
}

static void mark_free(uint64_t page) {
    if (page_used(page)) {
        bitmap[page / 8] &= ~(1u << (page % 8));
        free_count++;
    }
}

void pmm_init(struct limine_memmap_response *map, uint64_t hhdm_offset) {
    hhdm = hhdm_offset;

    uint64_t highest = 0;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->base + e->length > highest) {
            highest = e->base + e->length;
        }
    }

    page_count = highest / PAGE_SIZE;
    uint64_t bitmap_bytes = (page_count + 7) / 8;
    uint64_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t bitmap_phys = 0;
    bool found = false;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_pages * PAGE_SIZE) {
            bitmap_phys = e->base;
            found = true;
            break;
        }
    }
    if (!found) {
        panic("pmm: no room for the page bitmap");
    }

    bitmap = (uint8_t *)hhdm_phys_to_virt(bitmap_phys);
    memset(bitmap, 0xFF, bitmap_bytes);
    free_count = 0;

    for (uint64_t i = 0; i < map->entry_count; i++) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        for (uint64_t p = e->base / PAGE_SIZE; p < (e->base + e->length) / PAGE_SIZE; p++) {
            bitmap[p / 8] &= ~(1u << (p % 8));
            free_count++;
        }
    }
    total_usable = free_count;

    for (uint64_t p = 0; p < bitmap_pages; p++) {
        mark_used(bitmap_phys / PAGE_SIZE + p);
    }
    mark_used(0);  /* never hand out page 0 */
    search_hint = 0;
}

uint64_t pmm_alloc_contiguous(size_t pages) {
    uint64_t run = 0;
    for (uint64_t p = 0; p < page_count; p++) {
        if (page_used(p)) {
            run = 0;
            continue;
        }
        if (++run == pages) {
            uint64_t start = p + 1 - pages;
            for (uint64_t q = start; q <= p; q++) {
                mark_used(q);
            }
            return start * PAGE_SIZE;
        }
    }
    return 0;
}

uint64_t pmm_alloc_page(void) {
    for (uint64_t n = 0; n < page_count; n++) {
        uint64_t p = (search_hint + n) % page_count;
        if (!page_used(p)) {
            mark_used(p);
            search_hint = p + 1;
            return p * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint64_t phys) {
    mark_free(phys / PAGE_SIZE);
}

uint64_t pmm_total_pages(void) {
    return total_usable;
}

uint64_t pmm_free_pages(void) {
    return free_count;
}

/* ---- kernel heap: first-fit free list in one contiguous region ---- */

struct block {
    size_t size;          /* payload bytes */
    uint64_t free;
    struct block *next;
    uint64_t pad;         /* keeps payloads 32-byte aligned */
};

static struct block *heap_head;

void heap_init(void) {
    size_t pages = 4096;  /* 16 MiB, shrink if memory is tight */
    uint64_t phys = 0;
    while (pages >= 64 && !(phys = pmm_alloc_contiguous(pages))) {
        pages /= 2;
    }
    if (!phys) {
        panic("heap: could not reserve memory");
    }
    heap_head = (struct block *)hhdm_phys_to_virt(phys);
    heap_head->size = pages * PAGE_SIZE - sizeof(struct block);
    heap_head->free = 1;
    heap_head->next = NULL;
}

void *kmalloc(size_t size) {
    size = (size + 31) & ~(size_t)31;
    if (size == 0) {
        size = 32;
    }
    for (struct block *b = heap_head; b; b = b->next) {
        if (!b->free || b->size < size) {
            continue;
        }
        if (b->size >= size + sizeof(struct block) + 32) {
            struct block *rest = (struct block *)((uint8_t *)(b + 1) + size);
            rest->size = b->size - size - sizeof(struct block);
            rest->free = 1;
            rest->next = b->next;
            b->next = rest;
            b->size = size;
        }
        b->free = 0;
        return b + 1;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }
    ((struct block *)ptr - 1)->free = 1;
    for (struct block *b = heap_head; b && b->next; ) {
        if (b->free && b->next->free) {
            b->size += sizeof(struct block) + b->next->size;
            b->next = b->next->next;
        } else {
            b = b->next;
        }
    }
}

size_t heap_free_bytes(void) {
    size_t total = 0;
    for (struct block *b = heap_head; b; b = b->next) {
        if (b->free) {
            total += b->size;
        }
    }
    return total;
}
