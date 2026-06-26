# Feather RP2350 with Hazard3 RISC-V

The [Adafruit Feather RP2350](https://www.adafruit.com/product/6000) uses the RP2350A microcontroller with dual-core Arm Cortex-M33 and dual-core Hazard3 RISC-V processors.

## Board Specs

| Feature | Value |
|---------|-------|
| MCU | RP2350A |
| ARM Cores | 2x Cortex-M33 @ 150MHz |
| RISC-V Cores | 2x Hazard3 @ 150MHz |
| Flash | 8MB |
| PSRAM | None (RP2350A variant) |
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

The Hazard3 is an open-source RISC-V core implementing RV32IMAC with:
- `Zicsr` — Control and Status Register instructions
- `Zifencei` — Instruction fetch fence
- `Zba` — Address generation
- `Zbb` — Basic bit manipulation
- `Zbs` — Single-bit operations
- `Zbkb` — Bit manipulation for cryptography

### Build Configuration for Hazard3

| Setting | Value |
|---------|-------|
| Platform | `rp2350-riscv` |
| Compiler | `pico_riscv_gcc` |
| Triple | `riscv32-unknown-elf` |
| Arch | `rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb` |
| ABI | `ilp32` |
| Boot Stage2 | `compile_time_choice` (auto-detected from board) |

### Toolchain

The bundled toolchain lives at `deps/pico-sdk-tools/build/riscv-install/bin/`:
- `riscv32-unknown-elf-gcc` (GCC 15.2.0)
- `riscv32-unknown-elf-g++`
- `riscv32-unknown-elf-ld` (GNU ld.bfd)
- Full newlib standard library

The toolchain is referenced via `PICO_TOOLCHAIN_PATH` when running CMake.

## Core Selection

The RP2350 boots via the ARM Cortex-M33 ROM bootloader, then executes boot stage2. The stage2 configures the architecture selection register (ARCHSEL) to switch to Hazard3 RISC-V before jumping to the application.

To build for ARM instead, use platform `rp2350-arm-s` and the `arm-none-eabi-gcc` toolchain (not bundled; install via `apt install gcc-arm-none-eabi`).

## UF2 Bootloader

To enter bootloader mode:
1. Hold BOOTSEL button while plugging in USB, OR
2. Double-tap RESET while running, OR
3. Use picotool: `picotool reboot -f -u`

The board appears as a USB mass storage device. Copy `.uf2` files to flash.

## USB CDC Serial

The Feather uses the RP2350's built-in USB peripheral for serial output (no external USB-serial chip). The device appears as `/dev/ttyACM0` on Linux.

**Permission note:** You may need to be in the `dialout` group to access `/dev/ttyACM0`:
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

Or install udev rules for persistent access:
```bash
sudo cp deps/pico-sdk-tools/build/picotool/udev/60-picotool.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
```

## Known Issues

- **USB CDC on RISC-V**: The Hazard3 RISC-V core may require additional initialization for USB CDC to function reliably. If serial output is silent, try building for the ARM Cortex-M33 core (`rp2350-arm-s` platform) as a fallback.
- **No PSRAM on RP2350A**: The Feather uses the RP2350A variant without PSRAM. Keep heap usage within 520KB SRAM.
