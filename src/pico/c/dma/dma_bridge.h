/* DMA bridge for SagePico — exposes RP2350 DMA channels.
   Used for memory-to-PIO, framebuffer transfers, and async data movement.
   Included into generated hello.c by patch_main.py.
   Usage from REPL:
     let ch = dma_claim()
     dma_config(ch, src_addr, dst_addr, count, dreq)
     dma_start(ch)
*/

#ifndef SAGE_DMA_BRIDGE_H
#define SAGE_DMA_BRIDGE_H

#include "hardware/dma.h"

/* Claim first unused DMA channel. Returns channel number or -1. */
static int sage_dma_claim(void) {
    int ch = dma_claim_unused_channel(false);
    if (ch < 0) return -1;
    return ch;
}

/* Release a DMA channel */
static void sage_dma_unclaim(int ch) {
    dma_channel_unclaim(ch);
}

/* Configure DMA channel with default settings.
   dreq: DMA request (DREQ_PIO0_TX0, DREQ_PIO0_RX0, etc.)
   data_size: DMA_SIZE_8, DMA_SIZE_16, DMA_SIZE_32
   Returns the channel config for further customization */
static void sage_dma_config(int ch, uint32_t src, uint32_t dst, uint32_t count,
                             uint dreq, int data_size, int inc_read, int inc_write) {
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&c, (enum dma_channel_transfer_size)data_size);
    channel_config_set_read_increment(&c, inc_read);
    channel_config_set_write_increment(&c, inc_write);
    channel_config_set_dreq(&c, dreq);
    dma_channel_configure(ch, &c, (void*)dst, (const void*)src, count, false);
}

/* Start a DMA transfer */
static void sage_dma_start(int ch) {
    dma_channel_start(ch);
}

/* Wait for DMA transfer to complete (blocking) */
static void sage_dma_wait(int ch) {
    dma_channel_wait_for_finish_blocking(ch);
}

/* Check if DMA channel is busy */
static int sage_dma_busy(int ch) {
    return dma_channel_is_busy(ch);
}

/* Abort a DMA transfer */
static void sage_dma_abort(int ch) {
    dma_channel_abort(ch);
}

/* Set read address (for chaining/ring buffers) */
static void sage_dma_set_read(int ch, uint32_t addr) {
    dma_channel_set_read_addr(ch, (const void*)addr, false);
}

/* Set write address */
static void sage_dma_set_write(int ch, uint32_t addr) {
    dma_channel_set_write_addr(ch, (void*)addr, false);
}

/* Set transfer count */
static void sage_dma_set_count(int ch, uint32_t count) {
    dma_channel_set_trans_count(ch, count, false);
}

/* Chain DMA channel to another (fires target when source completes) */
static void sage_dma_chain_to(int ch, int target) {
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_chain_to(&c, target);
    /* re-apply config with chaining */
    dma_channel_set_config(ch, &c, false);
}

/* ---- DREQ constants ---- */
#define SAGE_DREQ_PIO0_TX0  DREQ_PIO0_TX0
#define SAGE_DREQ_PIO0_TX1  DREQ_PIO0_TX1
#define SAGE_DREQ_PIO0_TX2  DREQ_PIO0_TX2
#define SAGE_DREQ_PIO0_TX3  DREQ_PIO0_TX3
#define SAGE_DREQ_PIO0_RX0  DREQ_PIO0_RX0
#define SAGE_DREQ_PIO0_RX1  DREQ_PIO0_RX1
#define SAGE_DREQ_PIO0_RX2  DREQ_PIO0_RX2
#define SAGE_DREQ_PIO0_RX3  DREQ_PIO0_RX3
#define SAGE_DREQ_PIO1_TX0  DREQ_PIO1_TX0
#define SAGE_DREQ_PIO1_RX0  DREQ_PIO1_RX0
#define SAGE_DREQ_HSTX      DREQ_HSTX

/* ---- Data size constants ---- */
#define SAGE_DMA_SIZE_8  0
#define SAGE_DMA_SIZE_16 1
#define SAGE_DMA_SIZE_32 2

#endif
