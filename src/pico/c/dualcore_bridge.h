/* Dual-Core Bridge — DCP FIFO + doorbell */

#ifndef SAGE_DUALCORE_BRIDGE_H
#define SAGE_DUALCORE_BRIDGE_H

#include "hardware/structs/dcp.h"
#include "hardware/sync.h"

/* ---- DCP (Dual-Core Peripheral) ---- */
static int sage_dcp_send(uint32_t data) {
    if (dcp_hw->txfifo_level >= 4) return -1;
    dcp_hw->txfifo = data;
    return 0;
}
static uint32_t sage_dcp_recv(void) {
    if (dcp_hw->rxfifo_level == 0) return 0;
    return dcp_hw->rxfifo;
}
static void sage_dcp_notify(uint32_t mask) {
    dcp_hw->doorbell_out_set = mask;
}
static uint32_t sage_dcp_wait(void) {
    while (dcp_hw->doorbell_in == 0) tight_loop_contents();
    uint32_t val = dcp_hw->doorbell_in;
    dcp_hw->doorbell_in_clr = val;
    return val;
}
static uint32_t sage_dcp_tx_level(void) { return dcp_hw->txfifo_level; }
static uint32_t sage_dcp_rx_level(void) { return dcp_hw->rxfifo_level; }

#endif
