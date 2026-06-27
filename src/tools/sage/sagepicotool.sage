#!/usr/bin/env sage
# sagepicotool — Pure Sage picotool for RP2350
# Flash, info, reboot via USB BOOTSEL protocol.
# Requires: libsagepicotool.so (C helper with libusb)
# Build: cc -shared -fPIC -O2 -o libsagepicotool.so sagepicotool.c -lusb-1.0
#        sudo cp libsagepicotool.so /usr/local/lib/ && sudo ldconfig
# Usage: sage sagepicotool.sage info|load|reboot [--boot]

import sys

let libpt = nil
let args = sys.args()

# Parse command
let cmd = ""
let file_arg = ""
let to_bootsel = false

let i = 1
while i < len(args):
    let arg = args[i]
    if arg == "info":
        cmd = "info"
    else:
        if arg == "load":
            cmd = "load"
            if i + 1 < len(args):
                i = i + 1
                file_arg = args[i]
        else:
            if arg == "reboot":
                cmd = "reboot"
            else:
                if arg == "--boot":
                    to_bootsel = true
                else:
                    if not startswith(arg, "-") and cmd == "":
                        cmd = arg
    i = i + 1

if cmd == "":
    print "sagepicotool — RP2350 USB BOOTSEL tool (pure Sage)"
    print "Usage: sage sagepicotool.sage <command> [args]"
    print "  info              Show device information"
    print "  load <file.uf2>   Flash UF2 firmware"
    print "  reboot            Reboot device (--boot for BOOTSEL)"
    return

libpt = ffi_open("libsagepicotool.so")
if libpt == nil:
    print "Error: cannot open libsagepicotool.so"
    print "Build: cc -shared -fPIC -O2 -o libsagepicotool.so sagepicotool.c -lusb-1.0"
    print "       sudo cp libsagepicotool.so /usr/local/lib/ && sudo ldconfig"
    return

proc call_int(name, args):
    return ffi_call(libpt, name, "int", args)

proc call_str(name, args):
    return ffi_call(libpt, name, "string", args)

if cmd == "info":
    let info = call_str("sagepicotool_info", [])
    print info

else:
    if cmd == "load":
        if file_arg == "":
            print "Error: load requires a .uf2 file"
            return
        let r = call_int("sagepicotool_load", [file_arg])
        if r < 0:
            print "Error: load failed (device in BOOTSEL mode?)"
        else:
            print "Flashed: " + str(r) + " bytes"

    else:
        if cmd == "reboot":
            let boot_val = 0
            if to_bootsel:
                boot_val = 1
            let r = call_int("sagepicotool_reboot", [boot_val])
            if r < 0:
                print "Error: reboot failed"
            else:
                let msg = "Rebooting"
                if to_bootsel:
                    msg = msg + " to BOOTSEL"
                print msg + "..."

ffi_call(libpt, "sagepicotool_close", "void", [])
