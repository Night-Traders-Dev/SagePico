/* SRVM Binary Loader — loads .sgrv files into rvvm_t for execution.
   Parses SRVM format: magic "SGRV" + version + constant pool + chunks.
   Each chunk is a sequence of 32-bit RISC-V instructions. */

#ifndef SAGE_SRVM_LOADER_H
#define SAGE_SRVM_LOADER_H

#include "rvvm.h"
#include <string.h>

/* Read big-endian 32-bit value from byte buffer */
static inline uint32_t srvm_read32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  | (uint32_t)buf[3];
}
static inline uint16_t srvm_read16(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

typedef struct {
    uint8_t  type;      /* 0=number, 1=string */
    union {
        double  num;
        struct { uint16_t len; const uint8_t* data; } str;
    } value;
} srvm_const_t;

#define SRVM_MAX_CONSTS 64
#define SRVM_MAX_CHUNKS 8

typedef struct {
    uint32_t      magic;
    uint16_t      version;
    srvm_const_t  constants[SRVM_MAX_CONSTS];
    int           const_count;
    uint32_t      chunk_addrs[SRVM_MAX_CHUNKS];  /* PC addresses */
    uint32_t      chunk_sizes[SRVM_MAX_CHUNKS];
    int           chunk_count;
    uint8_t*      code_start;   /* pointer to first chunk data */
} srvm_program_t;

/* Parse .sgrv binary into srvm_program_t. Returns 1 on success. */
static int srvm_parse(const uint8_t* data, uint32_t len, srvm_program_t* prog) {
    memset(prog, 0, sizeof(*prog));
    if (len < 10) return 0;

    /* Magic */
    prog->magic = srvm_read32(data);
    if (prog->magic != 0x53475256) return 0; /* "SGRV" */
    const uint8_t* p = data + 4;

    /* Version */
    prog->version = srvm_read16(p); p += 2;

    /* Constant pool */
    uint16_t const_count = srvm_read16(p); p += 2;
    if (const_count > SRVM_MAX_CONSTS) const_count = SRVM_MAX_CONSTS;
    prog->const_count = const_count;

    for (int i = 0; i < const_count; i++) {
        if ((uint32_t)(p - data) >= len) return 0;
        uint8_t type = *p++;
        if (type == 0) { /* number (8 bytes double) */
            prog->constants[i].type = 0;
            uint64_t raw = ((uint64_t)srvm_read32(p) << 32) | srvm_read32(p + 4);
            memcpy(&prog->constants[i].value.num, &raw, 8);
            p += 8;
        } else if (type == 1) { /* string */
            prog->constants[i].type = 1;
            uint16_t slen = srvm_read16(p); p += 2;
            prog->constants[i].value.str.len = slen;
            prog->constants[i].value.str.data = p;
            p += slen;
        } else {
            return 0; /* unknown type */
        }
    }

    /* Chunk count */
    if ((uint32_t)(p - data) + 4 > len) return 0;
    uint32_t chunk_count = srvm_read32(p); p += 4;
    if (chunk_count > SRVM_MAX_CHUNKS) chunk_count = SRVM_MAX_CHUNKS;
    prog->chunk_count = chunk_count;
    prog->code_start = (uint8_t*)p;

    /* Parse chunk headers */
    uint32_t offset = 0;
    for (int i = 0; i < chunk_count; i++) {
        if ((uint32_t)(p - data) + 4 > len) return 0;
        uint32_t chunk_len = srvm_read32(p); p += 4;
        prog->chunk_addrs[i] = offset;
        prog->chunk_sizes[i] = chunk_len;
        offset += chunk_len;
        p += chunk_len;
    }

    return 1;
}

/* Load an SRVM program into the VM at a given chunk index.
   The chunk's instructions are copied to VM RAM starting at address 0.
   Returns 1 on success. */
static int srvm_load_chunk(rvvm_t* vm, srvm_program_t* prog, int chunk_idx) {
    if (chunk_idx < 0 || chunk_idx >= prog->chunk_count) return 0;

    const uint8_t* chunk_data = prog->code_start;
    /* Skip to the right chunk */
    for (int i = 0; i < chunk_idx; i++)
        chunk_data += 4 + prog->chunk_sizes[i];
    chunk_data += 4; /* skip chunk length header */

    uint32_t size = prog->chunk_sizes[chunk_idx];
    if (size > 65536) return 0;

    /* Copy instructions to VM RAM */
    memcpy(vm->ram, chunk_data, size);
    vm->pc = 0;
    vm->cycles = 0;
    vm->running = 1;
    return 1;
}

/* Load constant values into VM heap.
   Number constants go to heap key 0..N.
   String constants go to heap key 1000..1000+N (via length+pointer packed). */
static void srvm_load_constants(rvvm_t* vm, srvm_program_t* prog) {
    vm->heap_count = 0;
    for (int i = 0; i < prog->const_count && vm->heap_count < 64; i++) {
        if (prog->constants[i].type == 0) { /* number */
            uint32_t val;
            memcpy(&val, &prog->constants[i].value.num, 4);
            vm->heap_keys[vm->heap_count] = i;
            vm->heap_vals[vm->heap_count] = val;
            vm->heap_count++;
        }
    }
}

#endif
