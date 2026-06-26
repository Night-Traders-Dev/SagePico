#!/usr/bin/env sage
# sagecom — Pure Sage serial terminal for SagePico
# Connects to Feather RP2350 over USB CDC (/dev/ttyACM0)
# Usage: sage sagecom.sage [--port /dev/ttyACM0] [--baud 115200]
#        Ctrl+A then Q to quit
#
# Requires: libc FFI (works on desktop Linux)

import sys

let libc = nil
let port_path = "/dev/ttyACM0"
let baud_rate = 115200
let serial_fd = -1
let running = true
let stdin_is_tty = false

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
                print "sagecom — Sage serial terminal for SagePico"
                print "Usage: sage sagecom.sage [--port PATH] [--baud RATE]"
                print "  --port PATH   Serial device (default: /dev/ttyACM0)"
                print "  --baud RATE   Baud rate (default: 115200)"
                print ""
                print "Keys:"
                print "  Ctrl+A Q     Quit"
                print "  Ctrl+A C     Clear screen"
                print "  Ctrl+A R     Reset Feather (DTR toggle)"
                return
    i = i + 1

# ============================================================
# FFI helpers
# ============================================================

proc ffi_void(name, args):
    ffi_call(libc, name, "int", args)

proc ffi_int(name, args):
    return ffi_call(libc, name, "int", args)

# Allocate a termios buffer (60 bytes)
proc termios_alloc():
    return mem_alloc(60)

# Read termios c_cflag field (offset 8, 4 bytes)
proc termios_get_cflag(t):
    return mem_read(t, 8, "int")

# Write termios c_cflag field
proc termios_set_cflag(t, val):
    mem_write(t, 8, "int", val)

# Read termios c_lflag field (offset 12, 4 bytes)
proc termios_get_lflag(t):
    return mem_read(t, 12, "int")

# Write termios c_lflag field
proc termios_set_lflag(t, val):
    mem_write(t, 12, "int", val)

# Read termios c_iflag field (offset 0, 4 bytes)
proc termios_get_iflag(t):
    return mem_read(t, 0, "int")

# Write termios c_iflag field
proc termios_set_iflag(t, val):
    mem_write(t, 0, "int", val)

# Read termios c_oflag field (offset 4, 4 bytes)
proc termios_get_oflag(t):
    return mem_read(t, 4, "int")

# Write termios c_oflag field
proc termios_set_oflag(t, val):
    mem_write(t, 4, "int", val)

# Set baud rate in c_cflag
# B115200 = 0x1002 on most Linux, B921600 = 0x1007, etc.
proc baud_to_constant(rate):
    if rate == 9600:    return 0x0000000d
    if rate == 19200:   return 0x0000000e
    if rate == 38400:   return 0x0000000f
    if rate == 57600:   return 0x00001001
    if rate == 115200:  return 0x00001002
    if rate == 230400:  return 0x00001003
    if rate == 460800:  return 0x00001004
    if rate == 921600:  return 0x00001007
    return 0x00001002  # default 115200

# ============================================================
# Terminal setup
# ============================================================

# IOCTL constants (x86_64 Linux)
let TCGETS = 0x5401
let TCSETS  = 0x5402
let TCSETSW = 0x5403

# c_cflag bits
let CBAUD   = 0x0000100f
let CS8     = 0x00000030
let CREAD   = 0x00000080
let CLOCAL  = 0x00000800
let HUPCL   = 0x00004000

# c_lflag bits
let ICANON  = 0x00000002
let ECHO    = 0x00000008
let ECHOE   = 0x00000010
let ISIG    = 0x00000001
let IEXTEN  = 0x00008000

# c_iflag bits
let ICRNL   = 0x00000100
let IXON    = 0x00000400
let IXOFF   = 0x00001000
let BRKINT  = 0x00000002
let INPCK   = 0x00000010

# c_oflag bits
let OPOST   = 0x00000001
let ONLCR   = 0x00000004

# fcntl constants
let F_GETFL = 3
let F_SETFL = 4
let O_RDWR  = 2
let O_NOCTTY = 0x100
let O_NONBLOCK = 0x800

# Original terminal state to restore
let saved_termios = nil
let saved_termios_valid = false

proc save_stdin_termios():
    saved_termios = termios_alloc()
    let r = ffi_int("ioctl", [0, TCGETS, ptr_to_int(saved_termios)])
    if r == 0:
        saved_termios_valid = true
    else:
        saved_termios_valid = false
        print "Warning: could not save terminal settings"

proc restore_stdin_termios():
    if saved_termios_valid and saved_termios != nil:
        let r = ffi_int("ioctl", [0, TCSETSW, ptr_to_int(saved_termios)])
        if r != 0:
            print "Warning: could not restore terminal settings"
    if saved_termios != nil:
        mem_free(saved_termios)
        saved_termios = nil

proc set_raw_mode(fd):
    let t = termios_alloc()
    let addr = ptr_to_int(t)

    # Get current settings
    let r = ffi_int("ioctl", [fd, TCGETS, addr])
    if r != 0:
        print "Error: tcgetattr failed on fd " + str(fd)
        mem_free(t)
        return false

    # Modify for raw mode
    let cflag = termios_get_cflag(t)
    let lflag = termios_get_lflag(t)
    let iflag = termios_get_iflag(t)
    let oflag = termios_get_oflag(t)

    # Clear size bits, set 8N1
    cflag = cflag | CS8
    cflag = cflag | CREAD
    cflag = cflag | CLOCAL
    cflag = cflag & ~HUPCL  # no hangup on close

    # Turn off canonical, echo, signals
    lflag = lflag & ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN)

    # Turn off input processing
    iflag = iflag & ~(ICRNL | IXON | IXOFF | BRKINT | INPCK)

    # Turn off output processing
    oflag = oflag & ~(OPOST | ONLCR)

    termios_set_cflag(t, cflag)
    termios_set_lflag(t, lflag)
    termios_set_iflag(t, iflag)
    termios_set_oflag(t, oflag)

    # Apply
    r = ffi_int("ioctl", [fd, TCSETSW, addr])
    mem_free(t)

    if r != 0:
        print "Error: tcsetattr failed on fd " + str(fd)
        return false
    return true

proc configure_baud(fd, baud):
    let t = termios_alloc()
    let addr = ptr_to_int(t)

    let r = ffi_int("ioctl", [fd, TCGETS, addr])
    if r != 0:
        mem_free(t)
        return false

    let cflag = termios_get_cflag(t)
    let baud_const = baud_to_constant(baud)

    # Clear old baud, set new
    cflag = (cflag & ~CBAUD) | baud_const
    termios_set_cflag(t, cflag)

    r = ffi_int("ioctl", [fd, TCSETS, addr])
    mem_free(t)
    return r == 0

# ============================================================
# Output helpers
# ============================================================

# ANSI escape sequences
proc ansi(s):
    return "\x1b[" + s

let COLOR_RESET   = "\x1b[0m"
let COLOR_BOLD    = "\x1b[1m"
let COLOR_DIM     = "\x1b[2m"
let COLOR_GREEN   = "\x1b[32m"
let COLOR_YELLOW  = "\x1b[33m"
let COLOR_CYAN    = "\x1b[36m"
let COLOR_RED     = "\x1b[31m"

# Write to stdout
proc out(s):
    ffi_void("write", [1, s, len(s)])

# Write to serial port
proc serial_write(s):
    if serial_fd >= 0:
        ffi_int("write", [serial_fd, s, len(s)])

# Read from serial port (non-blocking, up to 256 bytes)
proc serial_read():
    if serial_fd < 0:
        return ""
    let buf = mem_alloc(256)
    let addr = ptr_to_int(buf)
    let n = ffi_int("read", [serial_fd, addr, 256])
    if n <= 0:
        mem_free(buf)
        return ""
    let result = mem_read(buf, 0, "string")
    mem_free(buf)
    return result

# Read from stdin (non-blocking, up to 256 bytes)
proc stdin_read():
    let buf = mem_alloc(256)
    let addr = ptr_to_int(buf)
    let n = ffi_int("read", [0, addr, 256])
    if n <= 0:
        mem_free(buf)
        return ""
    let result = mem_read(buf, 0, "string")
    mem_free(buf)
    return result

# ============================================================
# Status bar
# ============================================================

proc show_status():
    out(ansi("s"))  # save cursor
    out(ansi("H"))  # home
    out(ansi("K"))  # clear line
    out(COLOR_DIM)
    out("sagecom | " + port_path + " @ " + str(baud_rate) + " baud")
    out(" | Ctrl+A Q=quit C=clear R=reset")
    out(COLOR_RESET)
    out(ansi("u"))  # restore cursor

# ============================================================
# Main loop
# ============================================================

proc run():
    # Print banner
    out(ansi("2J"))  # clear screen
    out(ansi("H"))   # home
    out(COLOR_CYAN + COLOR_BOLD)
    out("╔══════════════════════════════════════════╗\n")
    out("║  sagecom — Sage Serial Terminal          ║\n")
    out("║  Target: " + port_path + "\n")
    out("║  Baud:   " + str(baud_rate) + "\n")
    out("║  Ctrl+A Q to quit                       ║\n")
    out("╚══════════════════════════════════════════╝\n")
    out(COLOR_RESET)
    out("\n")

    # Buffer for escape sequences from serial
    let serial_buf = ""
    # Ctrl+A escape state
    let escape_mode = false
    let frame = 0

    while running:
        # Read from serial
        let data = serial_read()
        if data != "":
            out(data)
            serial_buf = serial_buf + data

        # Read from stdin
        let input = stdin_read()
        if input != "":
            let i = 0
            while i < len(input):
                let c = input[i]
                if escape_mode:
                    escape_mode = false
                    if c == "Q" or c == "q":
                        out(COLOR_YELLOW + "\nExiting sagecom...\n" + COLOR_RESET)
                        running = false
                        break
                    else:
                        if c == "C" or c == "c":
                            out(ansi("2J") + ansi("H"))
                        else:
                            if c == "R" or c == "r":
                                out(COLOR_YELLOW + "Resetting Feather...\n" + COLOR_RESET)
                                # Toggle DTR to reset
                                ffi_int("ioctl", [serial_fd, 0x5423, 0])  # TIOCMSET: DTR=0
                                ffi_int("sleep", [1])  # careful: this calls sleep(1)
                                ffi_int("ioctl", [serial_fd, 0x5423, 1])  # DTR=1
                            else:
                                # Not a recognized escape - forward both
                                serial_write("\x01" + c)
                                out(COLOR_RED + "?" + COLOR_RESET)
                else:
                    if c == "\x01":  # Ctrl+A
                        escape_mode = true
                    else:
                        # Forward to serial
                        serial_write(c)
                i = i + 1

        # Status bar every 20 frames
        if frame % 20 == 0:
            show_status()

        # Small delay for non-blocking poll
        ffi_int("usleep", [10000])  # 10ms
        frame = frame + 1

# ============================================================
# Entry point
# ============================================================

# Initialize libc
libc = ffi_open("libc.so.6")
if libc == nil:
    print "Error: cannot open libc.so.6"
    return

# Check if stdin is a terminal
let isatty = ffi_int("isatty", [0])
if isatty == 1:
    stdin_is_tty = true
    save_stdin_termios()
    set_raw_mode(0)

# Open serial port
let flags = O_RDWR | O_NOCTTY | O_NONBLOCK
serial_fd = ffi_int("open", [port_path, flags])
if serial_fd < 0:
    print COLOR_RED + "Error: cannot open " + port_path + COLOR_RESET
    print "Is the Feather connected and in BOOTSEL or running firmware?"
    print "Check: ls -la " + port_path
    if stdin_is_tty:
        restore_stdin_termios()
    return

# Configure serial
configure_baud(serial_fd, baud_rate)
set_raw_mode(serial_fd)

print COLOR_GREEN + "Connected to " + port_path + " @ " + str(baud_rate) + " baud" + COLOR_RESET

# Run main loop
run()

# Cleanup
if serial_fd >= 0:
    ffi_int("close", [serial_fd])
    out("\n" + COLOR_DIM + "Serial port closed." + COLOR_RESET + "\n")

if stdin_is_tty:
    restore_stdin_termios()
    out(COLOR_DIM + "Terminal restored." + COLOR_RESET + "\n")
