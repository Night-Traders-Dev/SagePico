/* Hardware Interpolator bridge for SagePico.
   Exposes RP2350 interpolator (linear interpolation, fixed-point ops).
   Two interpolators, each with two lanes (accumulator + base/mask/shift).
   Usage: interp_config(lane, base, mask, shift), interp_pop(lane) */

#ifndef SAGE_INTERP_BRIDGE_H
#define SAGE_INTERP_BRIDGE_H

#include "hardware/interp.h"

static interp_hw_t* sage_interp_get(int idx) {
    return idx ? interp1_hw : interp0_hw;
}

/* Configure an interpolator lane.
   interp_idx: 0 or 1, lane: 0 or 1
   base: value added after each pop */
static void sage_interp_config(int interp_idx, int lane,
                                uint32_t base, uint32_t mask, uint8_t shift) {
    interp_hw_t* hw = sage_interp_get(interp_idx);
    interp_config c = interp_default_config();
    interp_config_set_shift(&c, shift);
    interp_config_set_mask(&c, mask, 0);
    interp_set_config(hw, lane, &c);
    hw->base[lane] = base;
}

/* Get latest interpolator result (pop from lane) */
static uint32_t sage_interp_pop(int interp_idx, int lane) {
    return interp_pop_lane_result(sage_interp_get(interp_idx), lane);
}

/* Force accumulator value */
static void sage_interp_force(int interp_idx, int lane, uint32_t val) {
    sage_interp_get(interp_idx)->accum[lane] = val;
}

/* Get raw accumulator value (without popping) */
static uint32_t sage_interp_peek(int interp_idx, int lane) {
    return sage_interp_get(interp_idx)->accum[lane];
}

#endif
