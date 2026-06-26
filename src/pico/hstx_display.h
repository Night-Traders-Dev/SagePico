/* HSTX DVI Display Driver for RP2350
   Drives a DVI/HDMI display via the HSTX peripheral.
   Uses TMDS encoding built into the RP2350 HSTX.
   Targets 640x480 @ 60Hz (standard VESA mode). */

#ifndef HSTX_DISPLAY_H
#define HSTX_DISPLAY_H

#include "pico/stdlib.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/dma.h"
#include <string.h>

/* ================================================================
   Display Configuration (640x480 @ 60Hz)
   ================================================================ */
#define DISP_WIDTH  640
#define DISP_HEIGHT 480
#define DISP_H_FP   16
#define DISP_H_SYNC 96
#define DISP_H_BP   48
#define DISP_H_TOTAL (DISP_WIDTH + DISP_H_FP + DISP_H_SYNC + DISP_H_BP)  // 800
#define DISP_V_FP   10
#define DISP_V_SYNC 2
#define DISP_V_BP   33
#define DISP_V_TOTAL (DISP_HEIGHT + DISP_V_FP + DISP_V_SYNC + DISP_V_BP)  // 525
#define DISP_PIXEL_CLOCK 25175000  // 25.175 MHz

/* HSTX FIFO base */
#define HSTX_FIFO_BASE 0x50120000

/* TMDS control tokens for DVI */
#define TMDS_CTL_BLANK     0x00
#define TMDS_CTL_HSYNC     0x02
#define TMDS_CTL_VSYNC     0x01
#define TMDS_CTL_ACTIVE    0x03

/* ================================================================
   Framebuffer (320 bytes per line, 16-bit RGB565)
   ================================================================ */
static uint16_t disp_framebuffer[DISP_HEIGHT][DISP_WIDTH] __attribute__((aligned(4)));

/* ================================================================
   HSTX Register Access
   ================================================================ */
static inline void hstx_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(HSTX_CTRL_BASE + offset) = value;
}

static inline uint32_t hstx_read(uint32_t offset) {
    return *(volatile uint32_t*)(HSTX_CTRL_BASE + offset);
}

static inline void hstx_fifo_push(uint32_t value) {
    while (!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_WOF_BITS)) { tight_loop_contents(); }
    hstx_fifo_hw->wdata = value;
}

static inline bool hstx_fifo_full(void) {
    return !!(hstx_fifo_hw->stat & HSTX_FIFO_STAT_WOF_BITS);
}

/* ================================================================
   DVI TMDS Encoding (simplified)
   ================================================================ */

/* Write a TMDS pixel pair to the HSTX FIFO.
   Each pixel pair is 2 pixels × 30 bits TMDS = 60 bits.
   The HSTX serializes this into 2×30-bit TMDS characters. */
static void hstx_write_pixel_pair(uint32_t p0, uint32_t p1, uint32_t ctl) {
    /* Simplified 8-bit RGB -> TMDS encode using HSTX hardware encoder.
       The HSTX can do TMDS encoding internally when configured for DVI mode.
       We write raw pixel+control data and the hardware handles the rest. */
    
    /* Pack two pixels into the FIFO format:
       Bits[29:20] = TMDS channel 2 (blue) for pixel 0
       Bits[19:10] = TMDS channel 1 (green) for pixel 0
       Bits[9:0]   = TMDS channel 0 (red) for pixel 0
       Next 30 bits = pixel 1 */
    
    /* For now, write raw RGB565 as placeholder.
       Full TMDS encoding requires lookup tables for 8b->10b. */
    uint32_t r0 = (p0 >> 11) & 0x1f;  // 5 bits red
    uint32_t g0 = (p0 >> 5) & 0x3f;   // 6 bits green
    uint32_t b0 = p0 & 0x1f;          // 5 bits blue
    uint32_t r1 = (p1 >> 11) & 0x1f;
    uint32_t g1 = (p1 >> 5) & 0x3f;
    uint32_t b1 = p1 & 0x1f;
    
    /* Scale to 8 bits */
    r0 = (r0 << 3) | (r0 >> 2);
    g0 = (g0 << 2) | (g0 >> 4);
    b0 = (b0 << 3) | (b0 >> 2);
    r1 = (r1 << 3) | (r1 >> 2);
    g1 = (g1 << 2) | (g1 >> 4);
    b1 = (b1 << 3) | (b1 >> 2);
    
    /* Write 60-bit TMDS pair */
    uint32_t w0 = (b0 << 20) | (g0 << 10) | r0;
    uint32_t w1 = (b1 << 20) | (g1 << 10) | (r1 << 10) | ctl;
    
    hstx_fifo_push(w0);
    hstx_fifo_push(w1);
}

/* ================================================================
   Display Initialization
   ================================================================ */

void display_init(void) {
    /* Enable HSTX clock from system PLL */
    clock_gpio_init_int(12, GPIO_FUNC_HSTX, 150000000);  // HSTX clock on GPIO 12
    clock_gpio_init_int(13, GPIO_FUNC_HSTX, 150000000);  // HSTX data on GPIO 13
    
    /* Reset HSTX */
    hstx_write(HSTX_CTRL_CSR_OFFSET, 0);
    sleep_us(10);
    
    /* Configure HSTX for DVI output:
       - 3 TMDS channels (RGB)
       - Pixel clock = 25.175 MHz
       - HSTX clock = pixel clock × 5 (for 10-bit TMDS × DDR) = 125.875 MHz
       Actually HSTX clock = pixel clock × 10 = 251.75 MHz for proper TMDS
       Using clk_sys / divisor */
    
    uint32_t csr = 0;
    csr |= (1u << HSTX_CTRL_CSR_CLKDIV_LSB);  // CLKDIV = 1 (HSTX clk / 2)
    csr |= (0u << HSTX_CTRL_CSR_CLKPHASE_LSB);
    csr |= HSTX_CTRL_CSR_EN_BITS;  // Enable
    hstx_write(HSTX_CTRL_CSR_OFFSET, csr);
}

/* ================================================================
   Framebuffer Render
   ================================================================ */

void display_render_frame(void) {
    for (int y = 0; y < DISP_HEIGHT; y++) {
        /* VSYNC at start */
        uint32_t ctl = (y < DISP_V_SYNC) ? TMDS_CTL_VSYNC : 
                        (y >= DISP_V_SYNC + DISP_V_BP && y < DISP_V_SYNC + DISP_V_BP + DISP_HEIGHT) ? TMDS_CTL_ACTIVE : TMDS_CTL_BLANK;
        
        for (int x = 0; x < DISP_WIDTH; x += 2) {
            uint32_t cx = (x < DISP_H_SYNC) ? TMDS_CTL_HSYNC :
                           (x >= DISP_H_SYNC + DISP_H_BP && x < DISP_H_SYNC + DISP_H_BP + DISP_WIDTH) ? TMDS_CTL_ACTIVE : TMDS_CTL_BLANK;
            
            uint16_t p0 = (ctl & cx) ? disp_framebuffer[y][x] : 0;
            uint16_t p1 = (ctl & cx) ? disp_framebuffer[y][x + 1] : 0;
            hstx_write_pixel_pair(p0, p1, cx);
        }
    }
}

/* ================================================================
   Drawing Functions
   ================================================================ */

static inline void display_clear(uint16_t color) {
    for (int y = 0; y < DISP_HEIGHT; y++)
        for (int x = 0; x < DISP_WIDTH; x++)
            disp_framebuffer[y][x] = color;
}

static inline void display_pixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < DISP_WIDTH && y >= 0 && y < DISP_HEIGHT)
        disp_framebuffer[y][x] = color;
}

/* RGB565 color helpers */
#define RGB565(r,g,b) ((((r)&0xf8)<<8)|(((g)&0xfc)<<3)|((b)>>3))
#define COLOR_WHITE  RGB565(255,255,255)
#define COLOR_BLACK  RGB565(0,0,0)
#define COLOR_RED    RGB565(255,0,0)
#define COLOR_GREEN  RGB565(0,255,0)
#define COLOR_BLUE   RGB565(0,0,255)

#endif
