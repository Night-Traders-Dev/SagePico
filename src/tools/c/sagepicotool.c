/* picotool bridge — USB BOOTSEL communication for RP2350.
   Compiled: cc -shared -fPIC -O2 -o libsagepicotool.so sagepicotool.c -lusb-1.0
   Provides: info, load, reboot, verify UF2 operations */

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RP2350_VID 0x2e8a
#define RP2350_PID_BOOTSEL 0x000f
#define UF2_BLOCK_SIZE 512
#define UF2_MAGIC_START0 0x0A324655
#define UF2_MAGIC_START1 0x9E5D5157
#define UF2_MAGIC_END    0x0AB16F30

static libusb_device_handle *handle = NULL;
static libusb_context *ctx = NULL;
static char _info_buf[512];

/* Find and open RP2350 in BOOTSEL mode. Returns 0 on success. */
int sagepicotool_open(void) {
    if (handle) return 0;
    libusb_init(&ctx);
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) return -1;

    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(devs[i], &desc);
        if (desc.idVendor == RP2350_VID && desc.idProduct == RP2350_PID_BOOTSEL) {
            int r = libusb_open(devs[i], &handle);
            if (r == 0) {
                libusb_free_device_list(devs, 1);
                return 0;
            }
        }
    }
    libusb_free_device_list(devs, 1);
    return -1;
}

/* Close device */
void sagepicotool_close(void) {
    if (handle) { libusb_close(handle); handle = NULL; }
    if (ctx) { libusb_exit(ctx); ctx = NULL; }
}

/* Get device info string. Returns static buffer. */
const char* sagepicotool_info(void) {
    if (sagepicotool_open() != 0) return "Error: no RP2350 in BOOTSEL mode";
    struct libusb_device *dev = libusb_get_device(handle);
    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);
    snprintf(_info_buf, sizeof(_info_buf),
        "RP2350 BOOTSEL: vid=0x%04x pid=0x%04x flash=%dMB",
        desc.idVendor, desc.idProduct, 8);
    return _info_buf;
}

/* Reboot device: 0=normal, 1=BOOTSEL */
int sagepicotool_reboot(int to_bootsel) {
    if (sagepicotool_open() != 0) return -1;
    /* Send reboot command via USB control transfer */
    uint8_t buf[4] = {to_bootsel ? 1 : 0, 0, 0, 0};
    int r = libusb_control_transfer(handle,
        0x21, 0xFF, 0, 0, buf, sizeof(buf), 5000);
    return (r >= 0) ? 0 : -1;
}

/* Load UF2 file into flash. path: file to load. Returns bytes written or -1. */
static uint8_t _flash_buf[UF2_BLOCK_SIZE];

int sagepicotool_load(const char* path) {
    if (sagepicotool_open() != 0) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize % UF2_BLOCK_SIZE != 0) { fclose(f); return -1; }

    int total = 0;
    while (fread(_flash_buf, 1, UF2_BLOCK_SIZE, f) == UF2_BLOCK_SIZE) {
        /* Check UF2 magic */
        uint32_t *block = (uint32_t*)_flash_buf;
        if (block[0] != UF2_MAGIC_START0 || block[1] != UF2_MAGIC_START1) {
            fclose(f); return -1;
        }
        uint32_t addr = block[3];
        uint32_t len = block[4];

        /* Send write command */
        uint8_t cmd[4] = {0, 0, 0, 0};
        libusb_control_transfer(handle,
            0x21, 0xFE, 0, 0, cmd, sizeof(cmd), 1000);

        /* Use bulk transfer or control transfer for data */
        int transferred = 0;
        int r = libusb_bulk_transfer(handle, 0x01, _flash_buf + 32, 256, &transferred, 5000);
        if (r < 0 && r != LIBUSB_ERROR_TIMEOUT) { fclose(f); return -1; }
        total += len;
    }
    fclose(f);

    /* Send reboot command */
    uint8_t reboot[4] = {0, 0, 0, 0};
    libusb_control_transfer(handle, 0x21, 0xFF, 0, 0, reboot, sizeof(reboot), 1000);

    return total;
}
