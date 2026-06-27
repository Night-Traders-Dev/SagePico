# sagepicotool — Pure Sage picotool

RP2350 USB BOOTSEL communication tool written in Sage. Flashes UF2 firmware, queries device info, and reboots.

## Architecture

```
sagepicotool.sage (Sage frontend)
    │ ffi_open("libsagepicotool.so")
    ▼
libsagepicotool.so (C bridge, 17K)
    │ libusb-1.0
    ▼
RP2350 BOOTSEL USB device (2e8a:000f)
```

The C bridge handles all USB control/bulk transfers via libusb. The Sage frontend provides the CLI and argument parsing. This split avoids pointer truncation issues in Sage's FFI (64-bit libusb pointers don't fit in 32-bit FFI int args).

## Installation

```bash
# Build C bridge
cd src/tools/c
cc -shared -fPIC -O2 -o libsagepicotool.so sagepicotool.c -lusb-1.0
sudo cp libsagepicotool.so /usr/local/lib/
sudo ldconfig

# Or use the pre-built binary
sudo cp tools/libsagepicotool.so /usr/local/lib/
sudo ldconfig
```

Requires: `libusb-1.0-dev` (`apt install libusb-1.0-0-dev`)

## Usage

```bash
sage src/tools/sage/sagepicotool.sage <command> [args]
```

### Commands

| Command | Description |
|---------|-------------|
| `info` | Show RP2350 device information (VID, PID, flash size) |
| `load <file.uf2>` | Flash UF2 firmware image |
| `reboot` | Reboot device to application mode |
| `reboot --boot` | Reboot device to BOOTSEL mode |

### Examples

```bash
# Check if device is in BOOTSEL
sage sagepicotool.sage info
# → RP2350 BOOTSEL: vid=0x2e8a pid=0x000f flash=8MB

# Flash firmware
sage sagepicotool.sage load build/hello-arm.uf2
# → Flashed: 86016 bytes

# Reboot to application
sage sagepicotool.sage reboot

# Reboot to BOOTSEL (for next flash)
sage sagepicotool.sage reboot --boot
```

## USB Protocol

The RP2350 BOOTSEL protocol uses USB control transfers:

### Device Identification
- VID: `0x2e8a` (Raspberry Pi)
- PID: `0x000f` (RP2350 BOOTSEL)

### Control Transfers

| Operation | bmRequestType | bRequest | Data |
|-----------|---------------|----------|------|
| Flash write | 0x21 (Host→Device, Class, Interface) | 0xFE | UF2 block (512 bytes) |
| Reboot | 0x21 | 0xFF | 1 byte (0=app, 1=BOOTSEL) |

### UF2 Block Format (512 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic start 0: 0x0A324655 |
| 4 | 4 | Magic start 1: 0x9E5D5157 |
| 8 | 4 | Flags (0x2000 = familyID present) |
| 12 | 4 | Target address (flash offset) |
| 16 | 4 | Payload size (256 bytes) |
| 20 | 4 | Block number |
| 24 | 4 | Total blocks |
| 28 | 4 | Family ID (RP2350 ARM = 0xe48bff59) |
| 32 | 256 | Payload data |
| 476 | 4 | Magic end: 0x0AB16F30 |

## C Bridge API

```c
int  sagepicotool_open(void);                    // Find and open BOOTSEL device
void sagepicotool_close(void);                   // Close device
const char* sagepicotool_info(void);             // Get device info string
int  sagepicotool_reboot(int to_bootsel);        // Reboot (0=app, 1=BOOTSEL)
int  sagepicotool_load(const char* path);        // Flash UF2 file
```

## Limitations

- Requires `sudo` or udev rules for USB access (same as picotool)
- Bulk transfer for flash data uses endpoint 0x01 (may need adjustment)
- No `verify` command (checksum verification not implemented)
- Flash address and family ID from UF2 metadata (no explicit address parameter)
- Single-file flash only (no multi-file or offset flashing)

## Comparison with picotool

| Feature | picotool (C++) | sagepicotool (Sage) |
|---------|---------------|---------------------|
| info | ✓ | ✓ |
| load | ✓ | ✓ |
| reboot | ✓ | ✓ |
| verify | ✓ | ✗ |
| save | ✓ | ✗ |
| seal | ✓ | ✗ |
| Binary size | ~500K | 17K (bridge) + Sage frontend |
| Dependencies | libusb, C++ runtime | libusb only |
