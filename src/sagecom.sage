#!/usr/bin/env sage
# sagecom — Sage serial terminal for SagePico
# Uses libsagecom_tty.so for safe TTY/serial I/O (no pointer truncation)
# Usage: sagecom [--port /dev/ttyACM0] [--baud 115200]
# Keys: Ctrl+A Q=quit  C=clear  R=reset

import sys

let libc = nil
let libtty = nil
let port_path = "/dev/ttyACM0"
let baud_rate = 115200
let serial_fd = -1
let running = true

# Parse command line
let i = 0
while i < len(sys.args()):
    let arg = sys.args()[i]
    if arg == "--port" and i + 1 < len(sys.args()):
        i = i + 1
        port_path = sys.args()[i]
    else:
        if arg == "--baud" and i + 1 < len(sys.args()):
            i = i + 1
            baud_rate = tonumber(sys.args()[i])
        else:
            if arg == "-h" or arg == "--help":
                print "sagecom — Sage Serial Terminal for SagePico"
                print "Usage: sagecom [--port PATH] [--baud RATE]"
                print "Keys: Ctrl+A Q=quit C=clear R=reset"
    i = i + 1

proc ffi_int(name, args):
    return ffi_call(libtty, name, "int", args)

proc ffi_str(name, args):
    return ffi_call(libtty, name, "string", args)

proc ffi_libc(name, args):
    return ffi_call(libc, name, "int", args)

let RESET  = "\x1b[0m"
let CYAN   = "\x1b[36m"
let GREEN  = "\x1b[32m"
let YELLOW = "\x1b[33m"
let RED    = "\x1b[31m"
let DIM    = "\x1b[2m"
let BOLD   = "\x1b[1m"

proc out(s):
    ffi_libc("write", [1, s, len(s)])

# ============================================================

libc = ffi_open("libc.so.6")
if libc == nil:
    libc = ffi_open("libc.so")
if libc == nil:
    print "Error: cannot open libc"
    return

libtty = ffi_open("libsagecom_tty.so")
if libtty == nil:
    print "Error: cannot open libsagecom_tty.so"
    print "Build: cc -shared -fPIC -O2 -o libsagecom_tty.so tools/sagecom_tty.c"
    print "       sudo cp libsagecom_tty.so /usr/local/lib/ && sudo ldconfig"
    return

# Setup terminal
let stdin_ok = ffi_int("sagecom_raw_stdin", [])
if stdin_ok < 0:
    out("(pipe mode) ")

# Open serial port — retry 20 times (5s)
let retries = 0
while serial_fd < 0 and retries < 20:
    serial_fd = ffi_int("sagecom_open_serial", [port_path, baud_rate])
    if serial_fd < 0:
        retries = retries + 1
        if retries == 1:
            out("Waiting for " + port_path + "...\n")
        ffi_libc("usleep", [250000])
if serial_fd < 0:
    out(RED + "Cannot open " + port_path + "\n" + RESET)
    ffi_int("sagecom_restore_stdin", [])
    return

# Drain any data that arrived during connection setup
ffi_call(libtty, "sagecom_serial_drain", "void", [serial_fd])
# Wait for any in-flight data to arrive, then drain again
ffi_libc("usleep", [500000])
ffi_call(libtty, "sagecom_serial_drain", "void", [serial_fd])

# Print banner atomically (single write call to minimize race window)
# Print status line (no box-drawing — terminal-agnostic)
let banner = CYAN + BOLD + "sagecom: " + port_path + " @ " + str(baud_rate) + " baud" + RESET + "\n" + DIM + "  ~. or ~q to quit, Ctrl+A C=clear R=reset" + RESET + "\n\n"
out(banner)

# ---- Main loop ----
let escape_mode = false
let frame = 0

while running:
    # Read from serial (string-based, no pointer args)
    let raw = ffi_str("sagecom_serial_read", [serial_fd])
    # Strip \r characters (Feather sends \r\n, we want just \n)
    let data = ""
    let j = 0
    while j < len(raw):
        let c = raw[j]
        if c != "\r":
            data = data + c
        j = j + 1
    if data != "":
        out(data)

    # Read from stdin (string-based)
    let input = ffi_str("sagecom_stdin_read", [])
    if input != "":
        let j = 0
        while j < len(input):
            let c = input[j]
            # ~ escape: ~. or ~q exits (works in line-buffered mode)
            if c == "~" and j == 0:
                if j + 1 < len(input) and (input[j+1] == "." or input[j+1] == "q"):
                    out(YELLOW + "\nExiting...\n" + RESET)
                    running = false
                    break
            if escape_mode:
                escape_mode = false
                if c == "Q" or c == "q":
                    out(YELLOW + "\nExiting...\n" + RESET)
                    running = false
                    break
                else:
                    if c == "C" or c == "c":
                        out("\x1b[2J\x1b[H")
                    else:
                        if c == "R" or c == "r":
                            out(YELLOW + "Reset...\n" + RESET)
                        else:
                            ffi_int("sagecom_serial_write", [serial_fd, "\x01" + c])
            else:
                if c == "\x01":
                    escape_mode = true
                else:
                    ffi_int("sagecom_serial_write", [serial_fd, c])
            j = j + 1

    ffi_libc("usleep", [10000])
    frame = frame + 1

# Cleanup
if serial_fd >= 0:
    ffi_libc("close", [serial_fd])
ffi_int("sagecom_restore_stdin", [])
out(DIM + "\nDisconnected.\n" + RESET)
