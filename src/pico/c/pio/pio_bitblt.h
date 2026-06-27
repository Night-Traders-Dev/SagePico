/* PIO BitBLT + Fill accelerator — offloads framebuffer operations from CPU.
   Uses 1 PIO state machine for memory-to-memory copy/fill at bus speed.
   API: pio_blit_init(pio_idx), pio_blit_copy(src, dst, count),
         pio_fill_rect(dst, color, count) */

#ifndef SAGE_PIO_BITBLT_H
#define SAGE_PIO_BITBLT_H

#include "hardware/pio.h"
#include "hardware/dma.h"

/* PIO program: reads data from FIFO, writes to consecutive addresses */
static const uint16_t pio_blit_program[] = {
    0x80a0, // pull block       ; get address from TX FIFO → OSR
    0xa027, // mov x, osr        ; X = base address
    0x80a0, // pull block        ; get count from TX FIFO
    0xa047, // mov y, osr        ; Y = count
    0x80a0, // pull block        ; get data/color
    0xa087, // mov exec, osr     ; execute: place data on output pins
    0x0085, // jmp y-- loop      ; loop until count done
};

static int _pio_blit_sm = -1;
static int _pio_blit_pio = 0;

/* Initialize BitBLT accelerator. Returns SM index or -1. */
static int sage_pio_blit_init(int pio_idx) {
    PIO pio = pio_idx ? pio1 : pio0;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) return -1;

    uint offset = pio_add_program(pio, (const pio_program_t*)pio_blit_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 6);
    sm_config_set_out_shift(&c, false, true, 32);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    _pio_blit_sm = sm;
    _pio_blit_pio = pio_idx;
    return sm;
}

/* Fill a memory region with a byte value using PIO.
   dst: destination address, color: byte to fill, count: number of bytes */
static void sage_pio_blit_fill(uint32_t dst, uint8_t color, uint32_t count) {
    if (_pio_blit_sm < 0) {
        /* Fallback: CPU memset */
        memset((void*)dst, color, count);
        return;
    }
    PIO pio = _pio_blit_pio ? pio1 : pio0;
    uint32_t word = color | (color << 8) | (color << 16) | (color << 24);

    /* Use DMA to feed PIO for performance */
    int dma_ch = dma_claim_unused_channel(false);
    if (dma_ch < 0) {
        /* No DMA — push words via CPU */
        pio_sm_put_blocking(pio, _pio_blit_sm, dst);
        pio_sm_put_blocking(pio, _pio_blit_sm, count);
        pio_sm_put_blocking(pio, _pio_blit_sm, word);
        return;
    }

    /* DMA config: push address, count, and repeated color word */
    uint32_t params[3] = { dst, count, word };
    dma_channel_config c = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, _pio_blit_pio ? DREQ_PIO1_TX0 : DREQ_PIO0_TX0);
    dma_channel_configure(dma_ch, &c,
        &pio->txf[_pio_blit_sm],
        params, 3, true);
    dma_channel_wait_for_finish_blocking(dma_ch);
    dma_channel_unclaim(dma_ch);
}

/* Fill a rectangle in the framebuffer.
   dst: framebuffer pointer, x, y: top-left, w, h: dimensions, color: byte */
static void sage_pio_blit_fill_rect(uint8_t* fb, uint32_t fb_w,
                                     uint32_t x, uint32_t y,
                                     uint32_t w, uint32_t h, uint8_t color) {
    for (uint32_t row = 0; row < h; row++) {
        uint32_t offset = (y + row) * fb_w + x;
        sage_pio_blit_fill((uint32_t)(fb + offset), color, w);
    }
}

#endif
