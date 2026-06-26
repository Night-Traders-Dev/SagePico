/* HSTX DVI Display Driver for Feather RP2350 + 7" Waveshare HDMI
   Uses hardware TMDS encoder (command expander).
   System clock bumped to 252 MHz for proper DVI timing.
   Touch: GT911 on I2C0 GPIO 22(SDA)/23(SCL)
   DDC: I2C1 GPIO 2(SDA)/3(SCL) — backlight control */

#ifndef HSTX_DISPLAY_H
#define HSTX_DISPLAY_H

#include "pico/stdlib.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/resets.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "hardware/i2c.h"
#include "hardware/vreg.h"
#include <string.h>

/* Display config: 640x400 internal FB, 2x scale to 1280x800 */
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

/* Framebuffer: 256-colour palette, 640x400 8bpp */
static uint16_t disp_palette[256] __attribute__((aligned(4)));
static uint8_t  disp_fb[FB_HEIGHT][FB_WIDTH] __attribute__((aligned(4)));

/* HSTX helpers */
static inline void hx_write(uint32_t off, uint32_t v) {
    *(volatile uint32_t*)(HSTX_CTRL_BASE + off) = v;
}
static inline void hx_fifo_wait(void) {
    volatile int t = 100000;
    while ((hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS) && --t) tight_loop_contents();
}
/* Push TMDS-encoded pixel via command expander (cmd=4 in bits[31:29]) */
static inline void hx_push_tmds(uint8_t r, uint8_t g, uint8_t b) {
    hx_fifo_wait();
    *(volatile uint32_t*)(HSTX_FIFO_BASE + 0x04) = (4u << 29) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
}

void display_init(void) {
    for (int i = 0; i < 256; i++) {
        uint8_t r = i & 0xe0, g = (i & 0x1c) << 3, b = (i & 0x03) << 6;
        disp_palette[i] = (uint16_t)(((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3));
    }
    memset(disp_fb, 0, sizeof(disp_fb));
    disp_palette[1]=0xffff; disp_palette[2]=0xf800; disp_palette[3]=0x07e0;
    disp_palette[4]=0x001f; disp_palette[5]=0xffe0; disp_palette[6]=0xf81f; disp_palette[7]=0x07ff;

    /* Boost voltage for 252 MHz */
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(252000, true);

    /* Unreset HSTX + enable clock */
    unreset_block_num_wait_blocking(RESET_HSTX);
    hw_set_bits(&clocks_hw->clk[clk_hstx].ctrl, CLOCKS_CLK_HSTX_CTRL_ENABLE_BITS);
    
    /* GPIO: 12=clk, 13=blue, 14=green, 15=red */
    gpio_set_function(12, GPIO_FUNC_HSTX);
    gpio_set_function(13, GPIO_FUNC_HSTX);
    gpio_set_function(14, GPIO_FUNC_HSTX);
    gpio_set_function(15, GPIO_FUNC_HSTX);

    /* TMDS expander: 8 bits per lane */
    hx_write(HSTX_CTRL_EXPAND_TMDS_OFFSET,
        (7u << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB) |
        (7u << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB) |
        (7u << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB));

    /* Enable HSTX with expander. CLKDIV=8 at 252MHz */
    hx_write(HSTX_CTRL_CSR_OFFSET, 0); sleep_us(100);
    hx_write(HSTX_CTRL_CSR_OFFSET,
        (8u << HSTX_CTRL_CSR_CLKDIV_LSB) | HSTX_CTRL_CSR_EXPAND_EN_BITS | HSTX_CTRL_CSR_EN_BITS);
}

/* Drawing */
static inline void disp_clear(uint8_t c) { memset(disp_fb, c, sizeof(disp_fb)); }
static inline void disp_rect(int x, int y, int w, int h, uint8_t c) {
    for (int dy=y; dy<y+h && dy<FB_HEIGHT; dy++)
        for (int dx=x; dx<x+w && dx<FB_WIDTH; dx++)
            disp_fb[dy][dx] = c;
}

/* GT911 Touch — I2C0 on GPIO 22(SDA)/23(SCL) */
#define GT911_ADDR 0x5D
static inline void gt911_init(void) {
    i2c_init(i2c0, 400000);
    gpio_set_function(22, GPIO_FUNC_I2C);
    gpio_set_function(23, GPIO_FUNC_I2C);
    gpio_pull_up(22); gpio_pull_up(23);
}
static inline int gt911_read(uint16_t *x, uint16_t *y, uint8_t *n) {
    uint8_t r[2]={0x81,0x4E}, st=0;
    if (i2c_write_blocking(i2c0,GT911_ADDR,r,2,true)<0) return -1;
    if (i2c_read_blocking(i2c0,GT911_ADDR,&st,1,false)<0) return -1;
    *n = (st & 0x80) ? (st & 0x0f) : 0;
    if (*n) {
        uint8_t d[5]; r[0]=0x81; r[1]=0x4F;
        if (i2c_write_blocking(i2c0,GT911_ADDR,r,2,true)<0) return -1;
        if (i2c_read_blocking(i2c0,GT911_ADDR,d,5,false)<0) return -1;
        *x=(d[1]<<8)|d[0]; *y=(d[3]<<8)|d[2];
    }
    uint8_t clr[3]={0x81,0x4E,0}; i2c_write_blocking(i2c0,GT911_ADDR,clr,3,false);
    return 0;
}

#endif
