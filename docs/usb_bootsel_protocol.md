# USB BOOTSEL Protocol Reference

Documented from the [RP2350 Bootrom source](https://github.com/raspberrypi/pico-bootrom-rp2350) (A4 release).

## RP2350 BOOTSEL Modes

The RP2350 has two USB personalities in BOOTSEL mode:

### 1. MSC (Mass Storage Class) — Default
- Appears as a FAT16 USB drive named `RPI-RP2`
- Contains `INDEX.HTM` and `INFO_UF2.TXT` virtual files
- UF2 flashing: write a `.uf2` file to the virtual drive
- The bootrom validates each 512-byte sector as a UF2 block
- After successful flash, auto-reboots to the new firmware
- VID: `0x2e8a`, PID: `0x0003` (MSC mode)

### 2. PICOBOOT (Vendor Protocol) — Alternative
- Raw USB control/bulk transfer protocol
- Used by `picotool` for low-level access
- VID: `0x2e8a`, PID: `0x000f`
- Supports: flash read/write/erase, reboot, info queries

## UF2 Block Format (512 bytes)

```
Offset  Size  Field             Value/Description
0x000   4     magic_start0       0x0A324655
0x004   4     magic_start1       0x9E5D5157
0x008   4     flags              UF2_FLAG_FAMILY_ID_PRESENT (0x2000)
0x00C   4     target_addr        Flash offset or RAM address
0x010   4     payload_size       256 (always)
0x014   4     block_no           Sequential block number
0x018   4     num_blocks         Total blocks in transfer
0x01C   4     family_id          RP2350_ARM_S or RP2350_RISCV
0x020   256   data               Payload (256 bytes of firmware)
0x120   4     magic_end          0x0AB16F30
```

### UF2 Flags

| Flag | Value | Description |
|------|-------|-------------|
| `UF2_FLAG_FAMILY_ID_PRESENT` | 0x2000 | Family ID field is valid |
| `UF2_FLAG_NOT_MAIN_FLASH` | 0x0001 | Data goes to RAM, not flash |
| `UF2_FLAG_FILE_CONTAINER` | 0x1000 | UF2 is embedded in a file |
| `UF2_FLAG_EXTENSION_FLAGS_PRESENT` | 0x4000 | Extension data follows payload |

### Family IDs

| ID | Architecture |
|----|-------------|
| `0xe48bff59` | RP2350 ARM-S (Cortex-M33) |
| `0xe48bff5a` | RP2350 RISC-V (Hazard3) |
| `0xe48bff5c` | Absolute address (RAM download) |

## Flash Process (from bootrom)

1. Device enumerates as MSC drive
2. Host copies `.uf2` file to virtual drive
3. Bootrom receives 512-byte sectors via SCSI WRITE(10)
4. Each sector validated: magic numbers, family ID, payload size
5. On first block of new transfer:
   - Reset state (num_blocks, family_id change)
   - Allocate bitmap for valid blocks
   - For flash: allocate sector erase bitmap
6. Each block written:
   - Check for duplicate (already written)
   - Queue async flash write task
   - Track progress in valid_blocks bitmap
7. On last block (all valid_blocks received):
   - Auto-reboot to new firmware
   - For flash: reboot via watchdog with REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE
   - For RAM: reboot with REBOOT2_FLAG_REBOOT_TYPE_RAM_IMAGE

## sagepicotool Protocol Usage

Our `sagepicotool` (C bridge + Sage frontend) uses the **PICOBOOT** vendor protocol:

| Operation | Transfer Type | Request | Data |
|-----------|--------------|---------|------|
| Info query | Control IN | 0x00 | Device info struct |
| Flash write | Control OUT | 0xFE | UF2 block (512 bytes) |
| Reboot | Control OUT | 0xFF | 1 byte (0=app, 1=BOOTSEL) |
| Flash erase | Control OUT | 0xFD | Sector address |
| Flash read | Control IN | 0xFC | Sector data |

## Verification Checklist

- [x] UF2 magic validation (start0, start1, end)
- [x] Family ID filtering (ARM-S, RISC-V, Absolute)
- [x] Sequential block numbering
- [x] Flash write with sector erase
- [x] Auto-reboot after successful flash
- [ ] Block deduplication (skip already-written blocks)
- [ ] Flash verify after write
- [ ] Partial transfer abort on family mismatch
- [ ] MSC mode support (virtual FAT16 drive)

## Bootrom Source Files

| File | Purpose |
|------|---------|
| `src/nsboot/usb_virtual_disk.c` | UF2 block validation, flash write task, MSC virtual disk |
| `src/nsboot/usb_msc.c` | USB Mass Storage Class implementation |
| `src/nsboot/nsboot_usb_client.c` | USB client setup (PICOBOOT protocol) |
| `src/nsboot/scsi.h` | SCSI command handling |
| `src/common/boot/uf2.h` | UF2 block structure definition |
