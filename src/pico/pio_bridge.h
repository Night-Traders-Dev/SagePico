/* PIO bridge for SagePico — exposes PIO state machines through native functions.
   Included into generated hello.c by patch_main.py.
   Usage from REPL:
     pio_claim_sm(0, 0)       # claim pio0, sm0
     pio_sm_put(0, 0, 0xFF)   # push byte to TX FIFO
     pio_sm_set_pins(0, 0, 25, 1)  # SM0 controls GPIO 25
*/

#ifndef SAGE_PIO_BRIDGE_H
#define SAGE_PIO_BRIDGE_H

#include "hardware/pio.h"
#include "hardware/clocks.h"

/* ---- PIO helpers ---- */

/* Claim a state machine, returns 1 on success */
static int sage_pio_claim(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    int idx = pio_claim_unused_sm(pio, false);
    if (idx < 0) return 0;
    if (idx != sm) { pio_sm_unclaim(pio, idx); return 0; }
    return 1;
}

/* Release a state machine */
static void sage_pio_unclaim(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_sm_unclaim(pio, sm);
}

/* Set SM pin directions and base pin */
static void sage_pio_sm_set_pins(int pio_idx, int sm, int pin_base, int pin_count, int out) {
    PIO pio = pio_idx ? pio1 : pio0;
    for (int i = 0; i < pin_count; i++) {
        pio_gpio_init(pio, pin_base + i);
        gpio_set_dir(pin_base + i, out ? GPIO_OUT : GPIO_IN);
    }
    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, pin_count, out);
}

/* Set SM clock divider. div = clk_sys / desired_freq */
static void sage_pio_sm_set_clkdiv(int pio_idx, int sm, float div) {
    PIO pio = pio_idx ? pio1 : pio0;
    uint16_t div_int = (uint16_t)div;
    uint8_t div_frac = (uint8_t)((div - div_int) * 256.0f);
    pio_sm_set_clkdiv_int_frac(pio, sm, div_int, div_frac);
}

/* Load and run a PIO program */
static int sage_pio_load_program(int pio_idx, const uint16_t* instructions, int len, int* offset_out) {
    PIO pio = pio_idx ? pio1 : pio0;
    if (!pio_can_add_program(pio, (const pio_program_t*)instructions))
        return 0;
    *offset_out = pio_add_program(pio, (const pio_program_t*)instructions);
    return 1;
}

/* Remove a PIO program */
static void sage_pio_remove_program(int pio_idx, const uint16_t* instructions, int offset) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_remove_program(pio, (const pio_program_t*)instructions, offset);
}

/* Push 32-bit word to TX FIFO */
static void sage_pio_sm_put(int pio_idx, int sm, uint32_t data) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_sm_put(pio, sm, data);
}

/* Read 32-bit word from RX FIFO (blocking) */
static uint32_t sage_pio_sm_get(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    return pio_sm_get(pio, sm);
}

/* Check if TX FIFO is full */
static int sage_pio_sm_tx_full(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    return pio_sm_is_tx_fifo_full(pio, sm);
}

/* Check if RX FIFO is empty */
static int sage_pio_sm_rx_empty(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    return pio_sm_is_rx_fifo_empty(pio, sm);
}

/* Start/stop SM */
static void sage_pio_sm_set_enabled(int pio_idx, int sm, int enabled) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_sm_set_enabled(pio, sm, enabled);
}

/* Restart SM from beginning of program */
static void sage_pio_sm_restart(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_sm_restart(pio, sm);
}

/* Clear TX/RX FIFOs */
static void sage_pio_sm_clear_fifos(int pio_idx, int sm) {
    PIO pio = pio_idx ? pio1 : pio0;
    pio_sm_clear_fifos(pio, sm);
}

/* Set PIO IRQ (0-7) */
static void sage_pio_set_irq(int pio_idx, int irq, int enabled) {
    PIO pio = pio_idx ? pio1 : pio0;
    if (enabled) {
        enum pio_interrupt_source mask = irq ? pis_sm0_tx_fifo_not_full : pis_interrupt0;
        pio_set_irqn_source_enabled(pio, irq, mask, true);
    }
}

/* ---- WS2812 (NeoPixel) high-level API ---- */

/* Standard WS2812 PIO program: one bit = T1H/T0H high + T1L/T0L low
   Clock: 8 MHz (125ns per cycle), T1H=6, T0H=3, T1L+T0L auto */
static const uint16_t ws2812_program[] = {
    0x6221, //  0: out x, 1    side 0 [2]
    0x1123, //  1: jmp !x, 3   side 1 [1]
    0x1402, //  2: jmp 2        side 1 [4]
    0xa442, //  3: nop          side 0 [4]
};

/* Initialize a GPIO pin for WS2812 output. Returns SM index or -1. */
static int sage_ws2812_init(int gpio_pin, int pio_idx) {
    PIO pio = pio_idx ? pio1 : pio0;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) return -1;

    uint offset = pio_add_program(pio, (const pio_program_t*)ws2812_program);
    if (offset < 0) { pio_sm_unclaim(pio, sm); return -1; }

    pio_gpio_init(pio, gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_OUT);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_sideset(&c, 1, 0, 0); // 1-bit sideset on GPIO pin
    sm_config_set_out_shift(&c, false, true, 24); // shift 24 bits out
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // TX only

    // Clock: 8 MHz for WS2812 (125ns per cycle)
    float div = (float)clock_get_hz(clk_sys) / 8000000.0f;
    sm_config_set_clkdiv(&c, div);

    sm_config_set_sideset_pins(&c, gpio_pin);
    sm_config_set_out_pins(&c, gpio_pin, 1);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    return sm;
}

/* Write GRB color to WS2812. Blocking until FIFO has space. */
static void sage_ws2812_put(int pio_idx, int sm, uint8_t r, uint8_t g, uint8_t b) {
    PIO pio = pio_idx ? pio1 : pio0;
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    pio_sm_put_blocking(pio, sm, grb << 8); // shift to align
}

/* Write array of GRB colors. data = [g,r,b, g,r,b, ...], count = number of LEDs */
static void sage_ws2812_write(int pio_idx, int sm, const uint8_t* data, int count) {
    PIO pio = pio_idx ? pio1 : pio0;
    for (int i = 0; i < count; i++) {
        uint8_t g = data[i * 3];
        uint8_t r = data[i * 3 + 1];
        uint8_t b = data[i * 3 + 2];
        uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
        pio_sm_put_blocking(pio, sm, grb << 8);
    }
}

#endif
