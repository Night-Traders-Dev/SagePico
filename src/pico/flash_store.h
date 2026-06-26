/* Flash-backed storage for SagePico — uses unused flash space for persistence.
   Wear-leveled log-structured storage. Survives power cycles.
   API (via FFI): flash_save(key, value_string), flash_load(key) -> value_string */

#ifndef SAGE_FLASH_STORE_H
#define SAGE_FLASH_STORE_H

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

/* Use upper 2MB of 8MB flash for swap/persistence (offset 6MB, 512 sectors) */
#define FLASH_STORE_OFFSET (6 * 1024 * 1024)
#define FLASH_STORE_SIZE   (2 * 1024 * 1024)
#define SFPK_SECTOR_SIZE   4096
#define SFPK_PAGE_SIZE     256
#define SFPK_MAX_ENTRIES   256
#define SFPK_MAGIC         0x5346504B  /* "SFPK" */

/* Each sector starts with a header, then key-value pairs */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sector_seq;   /* monotonic sequence for wear leveling */
    uint32_t entry_count;
    uint32_t crc;
} flash_sector_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t  key_len;
    uint16_t val_len;
    uint8_t  flags;        /* 0=active, 1=deleted */
    /* followed by key bytes, then value bytes */
} flash_entry_hdr_t;

static uint32_t _flash_seq = 0;
static uint32_t _flash_sector_addr = FLASH_STORE_OFFSET;
static int      _flash_initialized = 0;

/* Find the newest valid sector */
static int flash_find_active(void) {
    uint32_t best_seq = 0;
    uint32_t best_addr = FLASH_STORE_OFFSET;
    int found = 0;

    for (uint32_t addr = FLASH_STORE_OFFSET;
         addr < FLASH_STORE_OFFSET + FLASH_STORE_SIZE;
         addr += SFPK_SECTOR_SIZE) {
        const flash_sector_hdr_t* hdr = (const flash_sector_hdr_t*)(XIP_BASE + addr);
        if (hdr->magic == SFPK_MAGIC) {
            uint32_t seq = hdr->sector_seq;
            /* Handle wrap-around */
            if (!found || (seq - best_seq) < 0x80000000) {
                if (seq > best_seq || !found) {
                    best_seq = seq;
                    best_addr = addr;
                    found = 1;
                }
            }
        }
    }
    if (found) {
        _flash_seq = best_seq;
        _flash_sector_addr = best_addr;
    }
    return found;
}

/* Erase and initialize a fresh sector */
static void flash_erase_sector(uint32_t addr) {
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(addr, SFPK_SECTOR_SIZE);
    restore_interrupts(irq);

    flash_sector_hdr_t hdr = { .magic = SFPK_MAGIC, .sector_seq = _flash_seq, .entry_count = 0, .crc = 0 };
    uint8_t buf[SFPK_PAGE_SIZE] __attribute__((aligned(SFPK_PAGE_SIZE)));
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &hdr, sizeof(hdr));
    irq = save_and_disable_interrupts();
    flash_range_program(addr, buf, SFPK_PAGE_SIZE);
    restore_interrupts(irq);
}

/* Initialize flash store */
static void flash_store_init(void) {
    if (_flash_initialized) return;
    if (!flash_find_active()) {
        /* No valid sector found — start fresh */
        _flash_seq = 1;
        _flash_sector_addr = FLASH_STORE_OFFSET;
        flash_erase_sector(_flash_sector_addr);
    }
    _flash_initialized = 1;
}

/* Compact active entries to a new sector (garbage collection) */
static void flash_compact(void) {
    /* Read all active entries from current sector */
    struct { uint8_t key[64]; uint8_t val[256]; uint16_t vlen; } entries[SFPK_MAX_ENTRIES];
    int n = 0;

    const uint8_t* base = (const uint8_t*)(XIP_BASE + _flash_sector_addr);
    const flash_sector_hdr_t* hdr = (const flash_sector_hdr_t*)base;
    uint32_t offset = SFPK_PAGE_SIZE; /* skip header page */

    for (uint32_t i = 0; i < hdr->entry_count && n < SFPK_MAX_ENTRIES; i++) {
        const flash_entry_hdr_t* ehdr = (const flash_entry_hdr_t*)(base + offset);
        if (ehdr->key_len == 0 || ehdr->key_len > 63) break;
        offset += sizeof(flash_entry_hdr_t);
        if (ehdr->flags == 0) { /* active */
            memcpy(entries[n].key, base + offset, ehdr->key_len);
            entries[n].key[ehdr->key_len] = 0;
            offset += ehdr->key_len;
            uint16_t vl = ehdr->val_len;
            if (vl > 255) vl = 255;
            memcpy(entries[n].val, base + offset, vl);
            entries[n].vlen = vl;
            n++;
        }
        offset += ehdr->val_len;
        offset = (offset + 3) & ~3u; /* align */
        if (offset >= SFPK_SECTOR_SIZE - 256) break;
    }

    /* Move to next sector */
    _flash_sector_addr += SFPK_SECTOR_SIZE;
    if (_flash_sector_addr >= FLASH_STORE_OFFSET + FLASH_STORE_SIZE)
        _flash_sector_addr = FLASH_STORE_OFFSET;
    _flash_seq++;

    /* Erase and write new sector */
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(_flash_sector_addr, SFPK_SECTOR_SIZE);
    restore_interrupts(irq);

    uint8_t page[SFPK_PAGE_SIZE] __attribute__((aligned(SFPK_PAGE_SIZE)));
    memset(page, 0xFF, sizeof(page));
    flash_sector_hdr_t new_hdr = { .magic = SFPK_MAGIC, .sector_seq = _flash_seq, .entry_count = 0, .crc = 0 };
    memcpy(page, &new_hdr, sizeof(new_hdr));
    irq = save_and_disable_interrupts();
    flash_range_program(_flash_sector_addr, page, SFPK_PAGE_SIZE);
    restore_interrupts(irq);

    /* Rewrite active entries */
    uint32_t woff = SFPK_PAGE_SIZE;
    for (int i = 0; i < n; i++) {
        uint8_t klen = (uint8_t)strlen((const char*)entries[i].key);
        flash_entry_hdr_t e = { .key_len = klen, .val_len = entries[i].vlen, .flags = 0 };
        uint32_t elen = sizeof(e) + klen + entries[i].vlen;
        elen = (elen + 3) & ~3u;
        if (woff + elen > SFPK_SECTOR_SIZE - 256) break;

        memset(page, 0xFF, sizeof(page));
        memcpy(page, &e, sizeof(e));
        memcpy(page + sizeof(e), entries[i].key, klen);
        memcpy(page + sizeof(e) + klen, entries[i].val, entries[i].vlen);

        irq = save_and_disable_interrupts();
        flash_range_program(_flash_sector_addr + woff, page,
                           (elen + SFPK_PAGE_SIZE - 1) & ~(SFPK_PAGE_SIZE - 1));
        restore_interrupts(irq);
        woff += elen;
        new_hdr.entry_count++;
    }

    /* Update header with final count */
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &new_hdr, sizeof(new_hdr));
    irq = save_and_disable_interrupts();
    flash_range_program(_flash_sector_addr, page, SFPK_PAGE_SIZE);
    restore_interrupts(irq);
}

/* Save a key-value pair to flash */
static int flash_store_put(const char* key, const uint8_t* val, uint16_t vlen) {
    if (!_flash_initialized) flash_store_init();
    if (!key || strlen(key) > 63 || vlen > 255) return -1;

    uint8_t klen = (uint8_t)strlen(key);

    /* Calculate space needed */
    uint32_t needed = sizeof(flash_entry_hdr_t) + klen + vlen;
    needed = (needed + 3) & ~3u;

    /* Check available space in current sector */
    const flash_sector_hdr_t* hdr = (const flash_sector_hdr_t*)(XIP_BASE + _flash_sector_addr);
    uint32_t used = SFPK_PAGE_SIZE; /* header page */
    const uint8_t* base = (const uint8_t*)(XIP_BASE + _flash_sector_addr);
    for (uint32_t i = 0; i < hdr->entry_count; i++) {
        const flash_entry_hdr_t* ehdr = (const flash_entry_hdr_t*)(base + used);
        if (ehdr->key_len == 0) break;
        uint32_t esize = sizeof(flash_entry_hdr_t) + ehdr->key_len + ehdr->val_len;
        esize = (esize + 3) & ~3u;
        used += esize;
        if (used >= SFPK_SECTOR_SIZE - 256) break;
    }

    if (used + needed > SFPK_SECTOR_SIZE - 256) {
        flash_compact();
        /* Recalculate used after compaction */
        hdr = (const flash_sector_hdr_t*)(XIP_BASE + _flash_sector_addr);
        used = SFPK_PAGE_SIZE;
        base = (const uint8_t*)(XIP_BASE + _flash_sector_addr);
        for (uint32_t i = 0; i < hdr->entry_count; i++) {
            const flash_entry_hdr_t* ehdr = (const flash_entry_hdr_t*)(base + used);
            if (ehdr->key_len == 0) break;
            uint32_t esize = sizeof(flash_entry_hdr_t) + ehdr->key_len + ehdr->val_len;
            esize = (esize + 3) & ~3u;
            used += esize;
        }
    }

    /* Write entry */
    flash_entry_hdr_t e = { .key_len = klen, .val_len = vlen, .flags = 0 };
    uint32_t esize = sizeof(e) + klen + vlen;
    esize = (esize + 3) & ~3u;

    uint8_t page[SFPK_PAGE_SIZE] __attribute__((aligned(SFPK_PAGE_SIZE)));
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &e, sizeof(e));
    memcpy(page + sizeof(e), key, klen);
    memcpy(page + sizeof(e) + klen, val, vlen);

    uint32_t irq = save_and_disable_interrupts();
    flash_range_program(_flash_sector_addr + used, page,
                       (esize + SFPK_PAGE_SIZE - 1) & ~(SFPK_PAGE_SIZE - 1));
    restore_interrupts(irq);

    /* Update entry count in header */
    flash_sector_hdr_t new_hdr;
    memcpy(&new_hdr, hdr, sizeof(new_hdr));
    new_hdr.entry_count++;
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &new_hdr, sizeof(new_hdr));
    irq = save_and_disable_interrupts();
    flash_range_program(_flash_sector_addr, page, SFPK_PAGE_SIZE);
    restore_interrupts(irq);

    return 0;
}

/* Load a value by key from flash */
static int flash_store_get(const char* key, uint8_t* val_out, uint16_t* vlen_out) {
    if (!_flash_initialized) flash_store_init();
    if (!key) return -1;

    const uint8_t* base = (const uint8_t*)(XIP_BASE + _flash_sector_addr);
    const flash_sector_hdr_t* hdr = (const flash_sector_hdr_t*)base;
    uint32_t offset = SFPK_PAGE_SIZE;

    for (uint32_t i = 0; i < hdr->entry_count; i++) {
        const flash_entry_hdr_t* ehdr = (const flash_entry_hdr_t*)(base + offset);
        if (ehdr->key_len == 0) break;
        offset += sizeof(flash_entry_hdr_t);

        if (ehdr->flags == 0 &&
            ehdr->key_len == strlen(key) &&
            memcmp(base + offset, key, ehdr->key_len) == 0) {
            offset += ehdr->key_len;
            uint16_t vl = ehdr->val_len;
            if (vl > 255) vl = 255;
            if (val_out) memcpy(val_out, base + offset, vl);
            if (vlen_out) *vlen_out = vl;
            return 0;
        }
        offset += ehdr->key_len + ehdr->val_len;
        offset = (offset + 3) & ~3u;
    }
    return -1;
}

/* Delete a key */
static int flash_store_del(const char* key) {
    return flash_store_put(key, (const uint8_t*)"", 0); /* tombstone */
}

/* List all keys */
static void flash_store_keys(char keys[][64], int* count) {
    *count = 0;
    if (!_flash_initialized) flash_store_init();

    const uint8_t* base = (const uint8_t*)(XIP_BASE + _flash_sector_addr);
    const flash_sector_hdr_t* hdr = (const flash_sector_hdr_t*)base;
    uint32_t offset = SFPK_PAGE_SIZE; /* skip header */

    for (uint32_t i = 0; i < hdr->entry_count && *count < 64; i++) {
        const flash_entry_hdr_t* ehdr = (const flash_entry_hdr_t*)(base + offset);
        if (ehdr->key_len == 0) break;
        offset += sizeof(flash_entry_hdr_t);
        if (ehdr->flags == 0 && ehdr->key_len > 0 && ehdr->key_len < 64) {
            memcpy(keys[*count], base + offset, ehdr->key_len);
            keys[*count][ehdr->key_len] = 0;
            (*count)++;
        }
        offset += ehdr->key_len + ehdr->val_len;
        offset = (offset + 3) & ~3u;
    }
}

#endif
