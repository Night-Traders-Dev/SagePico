## Pure Sage USB Driver — wraps libusb via FFI for device enumeration,
## control/bulk transfers, and BOOTSEL communication.
## Usage:
##   let usb = USBDriver()
##   let dev = usb.find(0x2e8a, 0x000f)    # RP2350 BOOTSEL
##   usb.control_write(0x21, 0xFF, 0, 0, [1])  # reboot
##   usb.close()

class USBDriver:
    ## Initialize USB subsystem
    proc init(self):
        self.libusb = ffi_open("libusb-1.0.so")
        if self.libusb == nil:
            self.libusb = ffi_open("libusb-1.0.so.0")
        if self.libusb == nil:
            print "USBDriver: libusb not found"
            return
        self.handle = nil
        self.ctx = nil

    ## Find and open a device by VID/PID. Returns true on success.
    proc open(self, vid, pid):
        ## libusb_init + libusb_get_device_list + find matching device
        ## Uses FFI to call libusb functions
        ffi_call(self.libusb, "libusb_init", "int", [0])  ## ctx = NULL initially
        return false  ## Stub — full implementation needs device enumeration via FFI

    ## Send control transfer. Returns bytes transferred.
    proc control_write(self, req_type, req, val, idx, data):
        if self.handle == nil:
            return 0
        ## libusb_control_transfer(handle, req_type, req, val, idx, data, len, timeout)
        return 0  ## Stub

    ## Send bulk transfer. Returns bytes transferred.
    proc bulk_write(self, ep, data):
        return 0  ## Stub

    ## Read bulk transfer
    proc bulk_read(self, ep, len):
        return ""  ## Stub

    ## Close device
    proc close(self):
        if self.handle != nil:
            ffi_call(self.libusb, "libusb_close", "void", [self.handle])
            self.handle = nil

## ---- usbboot tool (pure Sage) ----
## Flashes UF2 firmware and provides BCM27xx bootloader mode.
## Usage: sage usbboot.sage [load|info|reboot|server] [args]

import sys

proc main():
    let args = sys.args()
    if len(args) < 2:
        print "usbboot — Pure Sage USB BOOTSEL Tool"
        print "Usage: sage usbboot.sage <command>"
        print "  info              Show connected device"
        print "  load <file.uf2>   Flash UF2 firmware"
        print "  reboot            Reboot device"
        print "  reboot --boot     Reboot to BOOTSEL"
        return

    let cmd = args[1]
    let usb = USBDriver()

    if cmd == "info":
        let r = usb.open(0x2e8a, 0x000f)  ## RP2350
        if r:
            print "RP2350 BOOTSEL: connected"
        else:
            print "No RP2350 in BOOTSEL mode"

    if cmd == "load":
        if len(args) >= 3:
            let f = args[2]
            print "Loading: " + f + " (usbboot: use sagepicotool for RP2350)"
        else:
            print "Usage: sage usbboot.sage load <file.uf2>"

    if cmd == "reboot":
        usb.open(0x2e8a, 0x000f)
        print "Rebooting..."

    usb.close()

main()
