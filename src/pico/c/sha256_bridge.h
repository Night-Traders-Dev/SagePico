/* SHA-256 hardware accelerator bridge for SagePico.
   Uses RP2350 hardware SHA-256 engine.
   API: sha256_hash(data, len) → 64-char hex string */

#ifndef SAGE_SHA256_BRIDGE_H
#define SAGE_SHA256_BRIDGE_H

#include "hardware/sha256.h"
#include <stdio.h>

/* Hash a byte buffer and return 64-char hex string (static buffer) */
static const char* sage_sha256_hash(const uint8_t* data, uint32_t len) {
    static char hex[65];
    sha256_result_t result;

    sha256_start();
    sha256_wait_ready_blocking();
    for (uint32_t i = 0; i < len; i++) {
        sha256_put_byte(data[i]);
    }
    sha256_wait_valid_blocking();
    sha256_get_result(&result, SHA256_LITTLE_ENDIAN);

    /* Format as hex */
    for (int i = 0; i < 32; i++) {
        snprintf(hex + i*2, 3, "%02x", result.bytes[i]);
    }
    hex[64] = 0;
    return hex;
}

/* Hash a null-terminated string */
static const char* sage_sha256_str(const char* s) {
    return sage_sha256_hash((const uint8_t*)s, (uint32_t)strlen(s));
}

#endif
