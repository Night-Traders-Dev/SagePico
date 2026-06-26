/* HSTX DVI Display Driver for RP2350 + Waveshare 7" 1280x800 HDMI
   CPU-driven scanout (no DMA yet). 640x400 internal 8bpp framebuffer.
   Touch: I2C GT911 on I2C0 pins 4(SDA)/5(SCL) */

#ifndef HSTX_DISPLAY_H
#define HSTX_DISPLAY_H

#include "pico/stdlib.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/resets.h"
#include "hardware/structs/clocks.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "hardware/i2c.h"
#include "hardware/platform.h"
#include <string.h>

/* ================================================================
   Display config: 640x400 internal, 2x scale to 1280x800
   ================================================================ */
#define FB_WIDTH  640
#define FB_HEIGHT 400

#define OUT_WIDTH   1280
#define OUT_HEIGHT  800
#define OUT_H_FP    48
#define OUT_H_SYNC  32
#define OUT_H_BP    80
#define OUT_H_TOTAL (OUT_WIDTH + OUT_H_FP + OUT_H_SYNC + OUT_H_BP)
#define OUT_V_FP    3
#define OUT_V_SYNC  6
#define OUT_V_BP    14
#define OUT_V_TOTAL (OUT_HEIGHT + OUT_V_FP + OUT_V_SYNC + OUT_V_BP)

/* HSTX FIFO base and write offset */
#define HSTX_FIFO_BASE       0x50120000
#define HSTX_FIFO_WDATA      (HSTX_FIFO_BASE + 0x04)

/* ================================================================
   Color palette (256 × RGB565) + 8bpp framebuffer (640×400 = 256KB)
   ================================================================ */
static uint16_t disp_palette[256] __attribute__((aligned(4)));
static uint8_t  disp_fb[FB_HEIGHT][FB_WIDTH] __attribute__((aligned(4)));

/* ================================================================
   HSTX helpers
   ================================================================ */
static inline void hx_write(uint32_t off, uint32_t v) {
    *(volatile uint32_t*)(HSTX_CTRL_BASE + off) = v;
}
static inline uint32_t hx_read(uint32_t off) {
    return *(volatile uint32_t*)(HSTX_CTRL_BASE + off);
}
static inline void hx_fifo_wait_not_full(void) {
    while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_WOF_BITS) tight_loop_contents();
}
static inline void hx_fifo_push(uint32_t v) {
    hx_fifo_wait_not_full();
    *(volatile uint32_t*)(HSTX_FIFO_WDATA) = v;
}

/* ================================================================
   Minimal TMDS encode (placeholder – needs proper LUT)
   ================================================================ */
static inline uint32_t tmds_enc_data(uint8_t d) {
    uint32_t n = ((d>>0)&1)+((d>>1)&1)+((d>>2)&1)+((d>>3)&1)
                +((d>>4)&1)+((d>>5)&1)+((d>>6)&1)+((d>>7)&1);
    uint32_t x = d;
    if (n > 4 || (n == 4 && !(d & 0x80))) { x = ~d & 0xff; }
    uint32_t t = (x & 0xff) << 2;
    return t | ((n > 4 || (n == 4 && !(d & 0x80))) ? 0x200 : 0);
}

static inline uint32_t tmds_enc_ctl(uint8_t c) {
    switch (c & 3) {
        case 0: return 0x354;
        case 1: return 0x154;
        case 2: return 0x2ab;
        case 3: return 0x0ab;
    }
    return 0;
}

/* ================================================================
   Display init
   ================================================================ */
void display_init(void) {
    for (int i = 0; i < 256; i++) {
        uint8_t r = i & 0xe0, g = (i & 0x1c) << 3, b = (i & 0x03) << 6;
        disp_palette[i] = (uint16_t)(((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3));
    }
    memset(disp_fb, 0, sizeof(disp_fb));

    /* Named palette indices */
    disp_palette[1]  = 0xffff;  /* white */
    disp_palette[2]  = 0xf800;  /* red */
    disp_palette[3]  = 0x07e0;  /* green */
    disp_palette[4]  = 0x001f;  /* blue */
    disp_palette[5]  = 0xffe0;  /* yellow */
    disp_palette[6]  = 0xf81f;  /* magenta */
    disp_palette[7]  = 0x07ff;  /* cyan */

    /* 1. Take HSTX out of reset */
    unreset_block_wait(RESET_HSTX);

    /* 2. Enable HSTX clock (CLK_HSTX_CTRL) */
    uint32_t clk_reg = CLOCKS_CLK_HSTX_CTRL_RESET;
    hw_write_masked(
        &clocks_hw->clk[clk_hstx].ctrl,
        CLOCKS_CLK_HSTX_CTRL_ENABLE_BITS,
        CLOCKS_CLK_HSTX_CTRL_ENABLE_BITS
    );

    /* 3. Set GPIO functions: 12=clk, 13-15=TMDS data lanes */
    gpio_set_function(12, GPIO_FUNC_HSTX);  /* clock */
    gpio_set_function(13, GPIO_FUNC_HSTX);  /* data 0 (blue) */
    gpio_set_function(14, GPIO_FUNC_HSTX);  /* data 1 (green) */
    gpio_set_function(15, GPIO_FUNC_HSTX);  /* data 2 (red) */

    /* 4. Configure HSTX CSR for TMDS/DVI output */
    hx_write(HSTX_CTRL_CSR_OFFSET, 0);  /* disable during config */
    sleep_us(100);

    /* CSR: CLKDIV=2 (150MHz/4=37.5MHz pixel clock), enable TMDS mode */
    uint32_t csr = (2u << HSTX_CTRL_CSR_CLKDIV_LSB)
                 | (0u << HSTX_CTRL_CSR_CLKPHASE_LSB)
                 | HSTX_CTRL_CSR_EN_BITS;
    hx_write(HSTX_CTRL_CSR_OFFSET, csr);
}

/* ================================================================
   Scanout (CPU driven — one frame)
   ================================================================ */
void display_render_frame(void) {
    for (int vy = 0; vy < OUT_V_TOTAL; vy++) {
        int v_active = (vy >= OUT_V_SYNC + OUT_V_BP && vy < OUT_V_SYNC + OUT_V_BP + OUT_HEIGHT);
        int v_sync   = (vy < OUT_V_SYNC);
        int fy       = v_active ? (vy - OUT_V_SYNC - OUT_V_BP) / 2 : 0;
        if (fy >= FB_HEIGHT) fy = FB_HEIGHT - 1;

        for (int hx = 0; hx < OUT_H_TOTAL; hx += 2) {
            int h_active = (hx >= OUT_H_SYNC + OUT_H_BP && hx < OUT_H_SYNC + OUT_H_BP + OUT_WIDTH);
            int h_sync   = (hx < OUT_H_SYNC);
            uint8_t ctl  = (h_sync ? 2 : 0) | (v_sync ? 1 : 0);
            int active   = v_active && h_active;

            uint32_t b0, g0, r0, b1, g1, r1;
            if (active) {
                int fx0 = (hx - OUT_H_SYNC - OUT_H_BP) / 2;
                int fx1 = fx0 + 1;
                if (fx0 >= FB_WIDTH) fx0 = FB_WIDTH-1;
                if (fx1 >= FB_WIDTH) fx1 = FB_WIDTH-1;
                uint16_t rgb0 = disp_palette[disp_fb[fy][fx0]];
                uint16_t rgb1 = disp_palette[disp_fb[fy][fx1]];
                b0 = tmds_enc_data((rgb0 << 3) & 0xf8);
                g0 = tmds_enc_data((rgb0 >> 3) & 0xfc);
                r0 = tmds_enc_data((rgb0 >> 8) & 0xf8);
                b1 = tmds_enc_data((rgb1 << 3) & 0xf8);
                g1 = tmds_enc_data((rgb1 >> 3) & 0xfc);
                r1 = tmds_enc_data((rgb1 >> 8) & 0xf8);
            } else {
                b0=g0=r0=b1=g1=r1=tmds_enc_ctl(ctl);
            }

            /* Interleave 2 pixels into 2×30-bit FIFO words */
            uint32_t w0 = (b0 & 0x3ff) | ((g0 & 0x3ff) << 10) | ((r0 & 0x3ff) << 20);
            uint32_t w1 = (b1 & 0x3ff) | ((g1 & 0x3ff) << 10) | ((r1 & 0x3ff) << 20);
            hx_fifo_push(w0);
            hx_fifo_push(w1);
        }
    }
}

/* ================================================================
   Drawing
   ================================================================ */
static inline void disp_clear(uint8_t c) { memset(disp_fb, c, sizeof(disp_fb)); }
static inline void disp_pixel(int x, int y, uint8_t c) {
    if ((unsigned)x < FB_WIDTH && (unsigned)y < FB_HEIGHT) disp_fb[y][x] = c;
}
static inline void disp_rect(int x, int y, int w, int h, uint8_t c) {
    for (int dy=y; dy<y+h && dy<FB_HEIGHT; dy++)
        for (int dx=x; dx<x+w && dx<FB_WIDTH; dx++)
            disp_fb[dy][dx] = c;
}
static inline void disp_line(int x0, int y0, int x1, int y1, uint8_t c) {
    int dx = x0<x1 ? x1-x0 : x0-x1, dy = y0<y1 ? y1-y0 : y0-y1;
    int sx = x0<x1 ? 1 : -1, sy = y0<y1 ? 1 : -1, e = dx-dy;
    while (1) {
        disp_pixel(x0,y0,c);
        if (x0==x1 && y0==y1) break;
        int e2=e*2; if(e2>-dy){e-=dy;x0+=sx;} if(e2<dx){e+=dx;y0+=sy;}
    }
}

/* ================================================================
   Touch (GT911 I2C)
   ================================================================ */
#define GT911_ADDR  0x5D
#define GT911_STAT  0x814E
#define GT911_PTS   0x814F

static inline void gt911_init(void) {
    i2c_init(i2c0, 400000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4); gpio_pull_up(5);
}

static inline int gt911_read(uint16_t *x, uint16_t *y, uint8_t *n) {
    uint8_t reg[2] = {GT911_STAT>>8, GT911_STAT&0xff}, st=0;
    if (i2c_write_blocking(i2c0,GT911_ADDR,reg,2,true)<0) return -1;
    if (i2c_read_blocking(i2c0,GT911_ADDR,&st,1,false)<0) return -1;
    *n = (st & 0x80) ? (st & 0x0f) : 0;
    if (*n) {
        uint8_t d[5]; reg[0]=GT911_PTS>>8; reg[1]=GT911_PTS&0xff;
        if (i2c_write_blocking(i2c0,GT911_ADDR,reg,2,true)<0) return -1;
        if (i2c_read_blocking(i2c0,GT911_ADDR,d,5,false)<0) return -1;
        *x = (d[1]<<8)|d[0]; *y = (d[3]<<8)|d[2];
    }
    reg[0]=GT911_STAT>>8; reg[1]=GT911_STAT&0xff;
    uint8_t zero[3]={GT911_STAT>>8,GT911_STAT&0xff,0};
    i2c_write_blocking(i2c0,GT911_ADDR,zero,3,false);
    return 0;
}

#endif
