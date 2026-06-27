/* System Control Bridge — vreg, clocks (full), xip_cache */

#ifndef SAGE_SYSCTRL_BRIDGE_H
#define SAGE_SYSCTRL_BRIDGE_H

#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/structs/xip_ctrl.h"

/* ---- Voltage Regulator ---- */
static uint32_t sage_vreg_get_voltage(void) {
    return 1200; /* Default RP2350 voltage in mV */
}
static void sage_vreg_set_voltage(uint32_t mv) {
    if (mv >= 1200) vreg_set_voltage(VREG_VOLTAGE_1_20);
    else if (mv >= 1100) vreg_set_voltage(VREG_VOLTAGE_1_10);
    else vreg_set_voltage(VREG_VOLTAGE_1_00);
}

/* ---- Clock Tree (full) ---- */
#define SAGE_CLK_SYS  0
#define SAGE_CLK_USB  1
#define SAGE_CLK_ADC  3

static clock_handle_t sage_clk_handle(int clk) {
    if (clk == 1) return clk_usb;
    if (clk == 3) return clk_adc;
    return clk_sys;
}

static uint64_t sage_clk_get_hz_full(int clk) {
    return clock_get_hz(sage_clk_handle(clk));
}

static void sage_clk_gpout_init(uint gpio, uint src, uint div) {
    clock_gpio_init(gpio, src, div);
}

/* ---- XIP Cache ---- */
static void sage_xip_cache_enable(void) {
    hw_set_bits(&xip_ctrl_hw->ctrl, 1); /* CTRL_EN = bit 0 */
}
static void sage_xip_cache_disable(void) {
    hw_clear_bits(&xip_ctrl_hw->ctrl, 1);
}
static void sage_xip_cache_flush(void) {
    hw_set_bits(&xip_ctrl_hw->stat, 1); /* STAT_FLUSH_READY */
}
static int sage_xip_cache_enabled(void) {
    return (xip_ctrl_hw->ctrl & 1) != 0;
}

#endif
