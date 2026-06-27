/* SHA-256 hardware accelerator bridge for SagePico — optimized.
   Uses word-at-a-time feeding (4x faster than byte-at-a-time).
   For bulk data (>256 bytes), use DMA via sha256_set_dma_size(). */

#ifndef SAGE_SHA256_BRIDGE_H
#define SAGE_SHA256_BRIDGE_H

#include "hardware/sha256.h"
#include "hardware/dma.h"
#include <stdio.h>

/* Hash a byte buffer using word-at-a-time feeding. Returns 64-char hex string. */
static const char* sage_sha256_hash(const uint8_t* data, uint32_t len) {
    static char hex[65];
    sha256_result_t result;

    sha256_start();
    sha256_wait_ready_blocking();

    /* Feed 32-bit words for speed (4 bytes per operation) */
    uint32_t i = 0;
    while (i + 4 <= len) {
        uint32_t word = ((uint32_t)data[i] << 24) | ((uint32_t)data[i+1] << 16) |
                        ((uint32_t)data[i+2] << 8) | data[i+3];
        sha256_put_word(word);
        i += 4;
    }
    /* Feed remaining bytes */
    while (i < len) {
        sha256_put_byte(data[i]);
        i++;
    }

    sha256_wait_valid_blocking();
    sha256_get_result(&result, SHA256_LITTLE_ENDIAN);

    for (int j = 0; j < 32; j++) {
        snprintf(hex + j*2, 3, "%02x", result.bytes[j]);
    }
    hex[64] = 0;
    return hex;
}

/* Hash using DMA for maximum throughput (good for >256 byte data).
   Requires a free DMA channel. Falls back to word-at-a-time if no DMA channel. */
static const char* sage_sha256_hash_dma(const uint8_t* data, uint32_t len) {
    static char hex[65];
    sha256_result_t result;

    int dma_ch = dma_claim_unused_channel(false);
    if (dma_ch < 0 || len < 256) {
        /* No DMA channel or small data — use word-at-a-time */
        if (dma_ch >= 0) dma_channel_unclaim(dma_ch);
        return sage_sha256_hash(data, len);
    }

    sha256_start();
    sha256_wait_ready_blocking();
    sha256_set_dma_size(len);

    /* Configure DMA to feed data into SHA-256 */
    dma_channel_config c = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_SHA256);
    dma_channel_configure(dma_ch, &c,
        &sha256_hw->wdata,     /* write to SHA-256 data register */
        data,                  /* read from input buffer */
        (len + 3) / 4,         /* transfer count in words */
        true                   /* start immediately */
    );

    dma_channel_wait_for_finish_blocking(dma_ch);
    dma_channel_unclaim(dma_ch);

    sha256_wait_valid_blocking();
    sha256_get_result(&result, SHA256_LITTLE_ENDIAN);

    for (int j = 0; j < 32; j++) {
        snprintf(hex + j*2, 3, "%02x", result.bytes[j]);
    }
    hex[64] = 0;
    return hex;
}

/* Hash a null-terminated string (word-at-a-time) */
static const char* sage_sha256_str(const char* s) {
    return sage_sha256_hash((const uint8_t*)s, (uint32_t)strlen(s));
}

#endif
