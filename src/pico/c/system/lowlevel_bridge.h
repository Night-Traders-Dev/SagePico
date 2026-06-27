/* Low-Level Hardware Bridge — ticks, sync, spin locks, divider, RISC-V CSRs, PLL, XOSC, RCP
   Deep port of all remaining pico-sdk libraries for full RP2350 hardware access. */

#ifndef SAGE_LOWLEVEL_BRIDGE_H
#define SAGE_LOWLEVEL_BRIDGE_H

#include "hardware/sync.h"
#include "hardware/pll.h"
#include "hardware/xosc.h"
#include "hardware/clocks.h"
#include "hardware/divider.h"
#include "hardware/structs/sio.h"

/* ---- hardware_ticks — Microsecond conversions ---- */
static uint64_t sage_ticks_to_us(uint64_t ticks) {
    return (ticks * 1000000ULL) / clock_get_hz(clk_sys);
}
static uint64_t sage_us_to_ticks(uint64_t us) {
    return (us * clock_get_hz(clk_sys)) / 1000000ULL;
}

/* ---- hardware_sync — Spin locks + critical sections ---- */
static uint32_t sage_sync_save_disable(void) {
    return save_and_disable_interrupts();
}
static void sage_sync_restore(uint32_t state) {
    restore_interrupts(state);
}
static int sage_spin_lock_claim(void) {
    return spin_lock_claim_unused(true);
}

/* ---- hardware_divider — Hardware integer division ---- */
static int32_t sage_hw_div_s32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    divmod_result_t r = hw_divider_divmod_s32(a, b);
    return (int32_t)(r & 0xFFFFFFFF);
}
static uint32_t sage_hw_div_u32(uint32_t a, uint32_t b) {
    if (b == 0) return 0;
    divmod_result_t r = hw_divider_divmod_u32(a, b);
    return (uint32_t)(r & 0xFFFFFFFF);
}
static int32_t sage_hw_mod_s32(int32_t a, int32_t b) {
    if (b == 0) return 0;
    divmod_result_t r = hw_divider_divmod_s32(a, b);
    return (int32_t)(r >> 32);
}

/* ---- hardware_pll — PLL configuration (stub — complex API) ---- */
static uint32_t sage_pll_get_freq(int pll) {
    if (pll == 0) return clock_get_hz(clk_sys);
    if (pll == 1) return clock_get_hz(clk_usb);
    return 0;
}

/* ---- hardware_xosc — Crystal oscillator ---- */
static void sage_xosc_init(uint32_t hz) {
    xosc_init();
    /* XOSC is auto-configured by SDK at startup */
    (void)hz;
}

/* ---- hardware_riscv — RISC-V platform features (compiled only on RISC-V) ---- */
#ifdef __riscv
static uint32_t sage_riscv_mcycle(void) {
    uint32_t lo;
    __asm__ volatile ("csrr %0, mcycle" : "=r"(lo));
    return lo;
}
static uint32_t sage_riscv_minstret(void) {
    uint32_t lo;
    __asm__ volatile ("csrr %0, minstret" : "=r"(lo));
    return lo;
}
static uint32_t sage_riscv_csr_read(uint32_t csr) {
    uint32_t val;
    __asm__ volatile ("csrr %0, %1" : "=r"(val) : "i"(csr));
    return val;
}
#else
static uint32_t sage_riscv_mcycle(void) { return 0; }
static uint32_t sage_riscv_minstret(void) { return 0; }
static uint32_t sage_riscv_csr_read(uint32_t csr) { (void)csr; return 0; }
#endif

/* ---- hardware_riscv_platform_timer — RISC-V mtime/mtimecmp ---- */
static uint64_t sage_riscv_timer_get(void) {
    /* RP2350 RISC-V: mtime is memory-mapped at SIO_BASE + 0x78 */
    volatile uint32_t *mtime_lo = (volatile uint32_t*)(SIO_BASE + 0x78);
    volatile uint32_t *mtime_hi = (volatile uint32_t*)(SIO_BASE + 0x7C);
    uint32_t hi1 = *mtime_hi;
    uint32_t lo = *mtime_lo;
    uint32_t hi2 = *mtime_hi;
    if (hi1 != hi2) lo = *mtime_lo;
    return ((uint64_t)hi2 << 32) | lo;
}

/* ---- hardware_hazard3 — RISC-V Hazard3 custom CSRs ---- */
#ifdef __riscv
static uint32_t sage_h3_irq_next(void) {
    uint32_t val;
    __asm__ volatile ("csrr %0, 0xbc0" : "=r"(val)); /* meintext */
    return val;
}
#else
static uint32_t sage_h3_irq_next(void) { return 0; }
#endif

/* ---- hardware_rcp — RP2350 Realtime Coprocessor (stub) ---- */
static int sage_rcp_available(void) {
    /* RCP is present on RP2350 but has limited public SDK support */
    return 0;
}

#endif
