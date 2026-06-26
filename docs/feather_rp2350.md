# Feather RP2350 Board Reference

The [Adafruit Feather RP2350](https://www.adafruit.com/product/6000) uses the RP2350A with dual Cortex-M33 ARM and dual Hazard3 RISC-V cores.

## Specifications

| Feature | Value |
|---------|-------|
| MCU | RP2350A |
| ARM Cores | 2x Cortex-M33 @ 150MHz |
| RISC-V Cores | 2x Hazard3 @ 150MHz |
| SRAM | 520KB |
| Flash | 8MB |
| USB | USB-C (native USB 1.1) |
| LED | GPIO 7 (red) |
| NeoPixel | GPIO 21 |

## Pinout

| Pin | Function | Notes |
|-----|----------|-------|
| 0 | UART0 TX | Default serial |
| 1 | UART0 RX | Default serial |
| 2 | I2C1 SDA | |
| 3 | I2C1 SCL | |
| 4 | I2C0 SDA | pico_port default |
| 5 | I2C0 SCL | pico_port default |
| 7 | LED | Red LED (active high) |
| 16 | SPI0 RX | MISO |
| 17 | SPI0 CSn | |
| 18 | SPI0 SCK | |
| 19 | SPI0 TX | MOSI |
| 20 | SPI0 RX alt | |
| 21 | NeoPixel | WS2812 |
| 22 | SPI0 SCK alt | |
| 23 | SPI0 TX alt | |
| 26-29 | ADC0-3 | Analog inputs |

## Build Configuration

### ARM (Cortex-M33) — Recommended

| Setting | Value |
|---------|-------|
| Platform | `rp2350-arm-s` |
| Compiler | `pico_arm_cortex_m33_gcc` |
| Triple | `arm-none-eabi` |
| Install | `apt install gcc-arm-none-eabi` |

### RISC-V (Hazard3) — Compiles, USB CDC WIP

| Setting | Value |
|---------|-------|
| Platform | `rp2350-riscv` |
| Compiler | `pico_riscv_gcc` |
| Triple | `riscv32-unknown-elf` |
| Arch | `rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb` |
| ABI | `ilp32` |
| Toolchain | Bundled in `deps/pico-sdk-tools/build/riscv-install/` |

## Core Selection

The RP2350 boots ARM ROM → boot stage2 → ARCHSEL register → application. The boot stage2 selects ARM or RISC-V based on the platform setting at build time. The board defaults to ARM (`PICO_PLATFORM=rp2350` → expanded to `rp2350-arm-s`). For RISC-V, set `PICO_PLATFORM=rp2350-riscv`.

## UF2 Bootloader

Enter bootloader mode:
1. Hold BOOTSEL while plugging USB
2. Double-tap RESET while running  
3. `picotool reboot -f -u`

Appears as USB mass storage drive. Copy `.uf2` to flash. Auto-reboots to application.

## USB CDC Serial

Device: `/dev/ttyACM0`. No external USB-serial chip — uses RP2350's built-in USB peripheral via TinyUSB.

**Permissions**: add user to `dialout` group or install udev rules:
```bash
sudo usermod -a -G dialout $USER   # then log out/in
```
