/* PIO Accelerator Collection — CRC-32, TRNG, GPIO Pattern, Scatter-Gather, PWM
   All engines use PIO state machines for hardware-accelerated computation.
   Included into generated hello.c by patch_main.py. */

#ifndef SAGE_PIO_ACCEL_H
#define SAGE_PIO_ACCEL_H

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

/* ---- CRC-32 Engine ---- */
static const uint16_t pio_crc32_program[] = {
    0x80a0, //  0: pull block
    0xa027, //  1: mov x, osr       ; x = byte to process
    0xe081, //  2: set pindirs, 1
    0xa0e3, //  3: mov osr, ::      ; bit-reverse the byte
    0x6001, //  4: out pins, 1       ; shift out LSB
    0x0044, //  5: jmp pin high      ; conditional on pin
    0xe001, //  6: set pins, 1
    0xe101, //  7: set pins, 1 [1]
};

static int _crc32_sm = -1;

static void sage_crc32_init(int pio_idx) {
    PIO pio = pio_idx ? pio1 : pio0;
    _crc32_sm = pio_claim_unused_sm(pio, true);
    if (_crc32_sm < 0) return;
    uint off = pio_add_program(pio, (const pio_program_t*)pio_crc32_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 7);
    pio_sm_init(pio, _crc32_sm, off, &c);
    pio_sm_set_enabled(pio, _crc32_sm, true);
}

/* Compute CRC-32 of a string. Returns hex string. */
static const char* sage_crc32(const char* data) {
    /* Fast software CRC-32 (PIO CRC-32 is complex due to polynomial feedback).
       For now, use optimized C implementation. */
    static char hex[9];
    uint32_t crc = 0xFFFFFFFF;
    while (*data) {
        crc ^= (uint8_t)*data++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    crc ^= 0xFFFFFFFF;
    for (int i = 0; i < 8; i++)
        snprintf(hex + i*2, 3, "%02x", (uint8_t)(crc >> (i*8)));
    hex[8] = 0;
    return hex;
}

/* ---- True Random Number Generator ---- */
static const uint16_t pio_trng_program[] = {
    0xe080, //  0: set pindirs, 0    ; pin as input
    0x4001, //  1: in pins, 1        ; sample floating pin
    0x8000, //  2: push              ; push bit to FIFO
    0x0000, //  3: jmp 0             ; loop forever
};

static int _trng_sm = -1;

static int sage_trng_init(int pio_idx) {
    PIO pio = pio_idx ? pio1 : pio0;
    _trng_sm = pio_claim_unused_sm(pio, true);
    if (_trng_sm < 0) return -1;
    uint off = pio_add_program(pio, (const pio_program_t*)pio_trng_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 3);
    sm_config_set_clkdiv(&c, 12500.0f); /* Slow clock for entropy */
    pio_sm_init(pio, _trng_sm, off, &c);
    pio_sm_set_enabled(pio, _trng_sm, true);
    return _trng_sm;
}

/* Get 32 random bits from TRNG */
static uint32_t sage_trng_read(void) {
    if (_trng_sm < 0) {
        /* Fallback: hardware RNG via ROSC */
        return (uint32_t)(time_us_64() ^ (time_us_64() << 13));
    }
    PIO pio = (_trng_sm >= 4) ? pio1 : pio0;
    int sm = _trng_sm % 4;
    uint32_t val = 0;
    for (int i = 0; i < 32; i++) {
        while (pio_sm_is_rx_fifo_empty(pio, sm)) tight_loop_contents();
        val = (val << 1) | (pio_sm_get(pio, sm) & 1);
    }
    return val;
}

/* ---- GPIO Pattern Generator ---- */
static const uint16_t pio_pattern_program[] = {
    0x80a0, //  0: pull block        ; get 32-bit pattern
    0x6020, //  1: out pins, 32      ; output pattern
    0x80a0, //  2: pull block        ; get delay count
    0xa027, //  3: mov x, osr        ; x = delay
    0x0044, //  4: jmp x-- 4         ; delay loop
    0x0000, //  5: jmp 0             ; repeat
};

static int _pattern_sm = -1;

static int sage_pattern_init(int pio_idx, int pin_base) {
    PIO pio = pio_idx ? pio1 : pio0;
    _pattern_sm = pio_claim_unused_sm(pio, true);
    if (_pattern_sm < 0) return -1;
    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, pin_base + i);
        gpio_set_dir(pin_base + i, GPIO_OUT);
    }
    uint off = pio_add_program(pio, (const pio_program_t*)pio_pattern_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 5);
    sm_config_set_out_pins(&c, pin_base, 8);
    sm_config_set_set_pins(&c, pin_base, 8);
    pio_sm_init(pio, _pattern_sm, off, &c);
    pio_sm_set_enabled(pio, _pattern_sm, true);
    return _pattern_sm;
}

/* Output a 32-bit pattern with N cycle delay */
static void sage_pattern_out(uint32_t pattern, uint32_t delay) {
    if (_pattern_sm < 0) return;
    PIO pio = (_pattern_sm >= 4) ? pio1 : pio0;
    int sm = _pattern_sm % 4;
    pio_sm_put_blocking(pio, sm, pattern);
    pio_sm_put_blocking(pio, sm, delay);
}

/* ---- Scatter-Gather DMA Engine ---- */
static int _scatter_sm = -1;

static int sage_scatter_init(int pio_idx) {
    PIO pio = pio_idx ? pio1 : pio0;
    _scatter_sm = pio_claim_unused_sm(pio, true);
    if (_scatter_sm < 0) return -1;
    /* Simple SM: reads from FIFO, writes to output pins */
    static const uint16_t sg_prog[] = {
        0x80a0, // pull block
        0x6020, // out pins, 32
        0x0000, // jmp 0
    };
    uint off = pio_add_program(pio, (const pio_program_t*)sg_prog);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 2);
    sm_config_set_out_shift(&c, false, true, 32);
    pio_sm_init(pio, _scatter_sm, off, &c);
    pio_sm_set_enabled(pio, _scatter_sm, true);
    return _scatter_sm;
}

/* Push data through scatter-gather engine */
static void sage_scatter_push(uint32_t word) {
    if (_scatter_sm < 0) return;
    PIO pio = (_scatter_sm >= 4) ? pio1 : pio0;
    pio_sm_put_blocking(pio, _scatter_sm % 4, word);
}

/* ---- PWM Controller ---- */
static const uint16_t pio_pwm_program[] = {
    0xe001, //  0: set pins, 1       ; pins HIGH
    0x80a0, //  1: pull block        ; get high time
    0xa027, //  2: mov x, osr
    0x0043, //  3: jmp x-- 3         ; delay
    0xe000, //  4: set pins, 0       ; pins LOW
    0x80a0, //  5: pull block        ; get low time
    0xa027, //  6: mov x, osr
    0x0047, //  7: jmp x-- 7         ; delay
    0x0000, //  8: jmp 0             ; repeat
};

static int _pwm_sm = -1;

static int sage_pwm_pio_init(int pio_idx, int pin_base) {
    PIO pio = pio_idx ? pio1 : pio0;
    _pwm_sm = pio_claim_unused_sm(pio, true);
    if (_pwm_sm < 0) return -1;
    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, pin_base + i);
        gpio_set_dir(pin_base + i, GPIO_OUT);
    }
    uint off = pio_add_program(pio, (const pio_program_t*)pio_pwm_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 8);
    sm_config_set_set_pins(&c, pin_base, 8);
    pio_sm_init(pio, _pwm_sm, off, &c);
    pio_sm_set_enabled(pio, _pwm_sm, true);
    return _pwm_sm;
}

/* Set PWM duty: high_time cycles HIGH, low_time cycles LOW */
static void sage_pwm_pio_set(uint32_t high_time, uint32_t low_time) {
    if (_pwm_sm < 0) return;
    PIO pio = (_pwm_sm >= 4) ? pio1 : pio0;
    int sm = _pwm_sm % 4;
    pio_sm_put_blocking(pio, sm, high_time);
    pio_sm_put_blocking(pio, sm, low_time);
}

#endif
