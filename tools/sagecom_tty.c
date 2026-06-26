/* sagecom_tty.c — C helper for serial port setup.
   Compiled as shared library: cc -shared -fPIC -o libsagecom_tty.so sagecom_tty.c
   Callable via Sage FFI without pointer truncation issues. */

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

static struct termios saved_stdin_termios;
static int saved_stdin_valid = 0;
static struct termios saved_serial_termios;
static int saved_serial_valid = 0;

/* Save and set raw mode on stdin (fd 0) */
int sagecom_raw_stdin(void) {
    if (!isatty(0)) return -2;  /* not a tty */
    if (tcgetattr(0, &saved_stdin_termios) < 0) return -1;
    saved_stdin_valid = 1;

    struct termios raw = saved_stdin_termios;
    raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);
    raw.c_iflag &= ~(ICRNL | IXON | IXOFF | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST | ONLCR);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSAFLUSH, &raw) < 0) return -1;
    return 0;
}

/* Restore stdin terminal settings */
int sagecom_restore_stdin(void) {
    if (!saved_stdin_valid) return -1;
    saved_stdin_valid = 0;
    return tcsetattr(0, TCSAFLUSH, &saved_stdin_termios);
}

/* Open and configure serial port. Returns fd or -1. */
int sagecom_open_serial(const char* path, int baud) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;

    /* Clear non-blocking after open */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) { close(fd); return -1; }

    /* Input flags: translate CR→NL, no other processing */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | IXON);
    tty.c_iflag |= ICRNL;  /* translate \r → \n */
    /* Output flags: raw */
    tty.c_oflag &= ~OPOST;
    /* Local flags: no echo, no canonical, no signals */
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    /* Control flags: 8N1, enable receiver, local */
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;

    /* Set baud rate */
    speed_t speed = B115200;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    /* Save for restore */
    saved_serial_termios = tty;
    saved_serial_valid = 1;

    /* Read with 100ms timeout: VMIN=0, VTIME=1 (1 decisecond) */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSAFLUSH, &tty) < 0) { close(fd); return -1; }
    return fd;
}

/* Read from serial port, returns pointer to static buffer (valid until next call).
   Returns empty string on error or no data. */
static char _serial_buf[256];
const char* sagecom_serial_read(int fd) {
    ssize_t n = read(fd, _serial_buf, sizeof(_serial_buf) - 1);
    if (n <= 0) return "";
    _serial_buf[n] = '\0';
    return _serial_buf;
}

/* Read from stdin, returns pointer to static buffer. */
static char _stdin_buf[256];
const char* sagecom_stdin_read(void) {
    ssize_t n = read(0, _stdin_buf, sizeof(_stdin_buf) - 1);
    if (n <= 0) return "";
    _stdin_buf[n] = '\0';
    return _stdin_buf;
}

/* Write to serial port, returns bytes written or -1 */
int sagecom_serial_write(int fd, const char* data) {
    return (int)write(fd, data, strlen(data));
}
