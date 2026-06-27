# Future Porting Roadmap

Remaining pico-sdk libraries to port to pure Sage, ordered by practical utility.

## System Control (Medium Priority)

### `hardware_vreg`
Voltage regulator control. The RP2350 has an on-chip voltage regulator that can be adjusted for overclocking.

**Planned API**: `vreg_get_voltage()`, `vreg_set_voltage(mv)`

**Use case**: Dynamic voltage scaling for power efficiency or overclocking.

### `hardware_powman`
Power management — sleep, dormant, and deep-sleep modes.

**Planned API**: `powman_sleep_ms(ms)`, `powman_sleep_until_pin(pin, edge)`, `powman_dormant(pin, edge)`

**Use case**: Battery-powered applications, wake-on-interrupt.

### `hardware_xip_cache`
XIP (Execute-In-Place) cache control. The RP2350 has a 16KB cache for flash access.

**Planned API**: `xip_cache_enable()`, `xip_cache_disable()`, `xip_cache_flush()`, `xip_cache_stats()`

**Use case**: Performance tuning, cache-aware programming.

### `hardware_clocks`
Full clock tree configuration — currently partial (only `clk_get_hz`).

**Planned API**: `clk_set_divider(clock, div)`, `clk_gpout_init(pin, src, div)`, `clk_get_hz(clock)`

**Use case**: Custom clock outputs, frequency measurement.

### `hardware_exception`
Exception and fault handlers.

**Planned API**: Exception handler registration for HardFault, MemManage, BusFault, UsageFault.

**Use case**: Crash diagnostics, fault recovery.

## Dual-Core (Medium Priority)

### `hardware_dcp`
Dual-core communication — FIFOs and doorbell interrupts between Cortex-M33 and Hazard3 cores.

**Planned API**: `dcp_send(data)`, `dcp_recv()`, `dcp_notify()`, `dcp_wait()`

**Use case**: Multi-core firmware, core-to-core messaging.

### `hardware_boot_lock`
Low-level boot synchronization between cores.

**Planned API**: Boot sequencing for dual-core startup.

**Use case**: Multi-core initialization.

## Cryptographic (Medium Priority)

### `hardware_sha256` — ✓ Already ported

## Specialized Hardware (Low Priority)

### `hardware_rcp`
RP2350 Realtime Coprocessor (new for RP2350) — dedicated hardware for real-time tasks.

**Planned API**: TBD (documentation pending)

**Use case**: Hard real-time control loops.

### `hardware_interpolator` — ✓ Already ported

### `hardware_divider`
Hardware integer division accelerator. Used implicitly by the C compiler for `/` and `%` operations.

**Planned API**: Not needed for Sage — used automatically by generated C code.

**Use case**: Performance (transparent to user code).

## Platform-Specific (Low Priority)

### `hardware_hazard3`
RISC-V Hazard3 custom CSRs and features.

**Planned API**: Hazard3-specific register access.

**Use case**: RISC-V-only advanced features.

### `hardware_riscv`
RISC-V platform features.

**Planned API**: `riscv_mcycle()`, `riscv_minstret()`, CSR read/write.

**Use case**: Performance counters, RISC-V diagnostics.

### `hardware_riscv_platform_timer`
RISC-V machine timer (`mtime`/`mtimecmp`).

**Planned API**: `riscv_timer_get()`, `riscv_timer_set_cmp(val)`.

**Use case**: RISC-V timer interrupts.

## Clock & Oscillator (Low Priority)

### `hardware_pll`
Phase-Locked Loop configuration for system, USB, and ADC clocks.

**Planned API**: `pll_get_freq(pll)`, `pll_set_freq(pll, hz)`.

**Use case**: Custom clock frequencies.

### `hardware_xosc`
Crystal oscillator configuration.

**Planned API**: `xosc_init(hz)`, `xosc_disable()`.

**Use case**: External crystal setup.

### `hardware_ticks`
Microsecond tick conversion utilities.

**Planned API**: `ticks_to_us(ticks)`, `us_to_ticks(us)`.

**Use case**: Precise timing calculations.

## Synchronization (Low Priority)

### `hardware_sync`
Spin locks and critical sections (partially available via C bridge).

**Planned API**: `sync_lock(n)`, `sync_unlock(n)`, `sync_save_disable()`, `sync_restore(state)`.

**Use case**: Multi-core synchronization.

### `hardware_sync_spin_lock`
Low-level hardware spin lock primitives.

**Planned API**: Direct spin lock register access.

**Use case**: Minimal-latency locking.

## Porting Priority Summary

| Priority | Libraries | Reason |
|----------|-----------|--------|
| **High** | — | All high-priority libraries are ported |
| **Medium** | vreg, powman, xip_cache, clocks(full), exception, dcp, boot_lock | System control and dual-core |
| **Low** | rcp, divider, hazard3, riscv, pll, xosc, ticks, sync, spin_lock | Specialized or already available implicitly |

## Current Porting Status

- **Fully ported (FFI + Sage wrapper)**: 14 libraries
- **Partially ported (C bridge)**: 6 libraries
- **Not yet ported**: 15 libraries
- **Total pico-sdk hardware libraries**: 35
- **Porting completion**: 57% (20 of 35 have at least C bridge access)
