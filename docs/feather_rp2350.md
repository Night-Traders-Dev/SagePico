# Feather RP2350 with Hazard3 RISC-V

The [Adafruit Feather RP2350](https://www.adafruit.com/product/6000) is a development board featuring the Raspberry Pi RP2350B microcontroller with dual-core Arm Cortex-M33 and dual-core Hazard3 RISC-V processors.

## Board Specs

| Feature | Value |
|---------|-------|
| MCU | RP2350B |
| ARM Cores | 2x Cortex-M33 @ 150MHz |
| RISC-V Cores | 2x Hazard3 @ 150MHz |
| Flash | 8MB |
| PSRAM | 8MB (on RP2350B variant) |
| USB | USB-C (native USB 1.1) |
| GPIO | 21 pins (Feather format) |
| LED | GPIO 7 (red) |
| NeoPixel | GPIO 21 |

## Pinout

| Pin | Function | Notes |
|-----|----------|-------|
| 0 | UART0 TX | Default serial TX |
| 1 | UART0 RX | Default serial RX |
| 2 | I2C1 SDA | Default I2C data |
| 3 | I2C1 SCL | Default I2C clock |
| 7 | LED | Red LED (active high) |
| 20 | SPI0 RX | Default SPI MISO |
| 21 | NeoPixel | WS2812 addressable LED |
| 22 | SPI0 SCK | Default SPI clock |
| 23 | SPI0 TX | Default SPI MOSI |

## Hazard3 RISC-V Architecture

The Hazard3 is an open-source RISC-V core implementing RV32IMAC (with Zicsr, Zifencei, Zba, Zbb, Zbs, Zbkb extensions). The RP2350 pairs two Hazard3 cores alongside two Cortex-M33 cores, toggled at boot time.

To build for Hazard3:
- **Platform**: `rp2350-riscv`
- **Compiler**: `pico_riscv_gcc`
- **Triple**: `riscv32-unknown-elf`
- **Arch flags**: `-march=rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb -mabi=ilp32`

## UF2 Bootloader

The RP2350 uses a UF2 bootloader. Enter bootloader mode by:
1. Hold BOOTSEL button while plugging in USB
2. Or double-tap RESET while running

The board appears as a mass storage device. Copy the `.uf2` file to flash.
