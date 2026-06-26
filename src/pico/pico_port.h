/* Sage-to-Pico SDK bridge - lightweight C wrappers for Pico peripherals */
#ifndef SAGE_PICO_PORT_H
#define SAGE_PICO_PORT_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <stdio.h>

/* ================================================================
   GPIO
   ================================================================ */
#define SAGE_GPIO_OUT 1
#define SAGE_GPIO_IN  0

static inline void sage_gpio_init(int pin)           { gpio_init(pin); }
static inline void sage_gpio_set_dir(int pin, int d)  { gpio_set_dir(pin, d); }
static inline void sage_gpio_put(int pin, int v)      { gpio_put(pin, v); }
static inline int  sage_gpio_get(int pin)             { return gpio_get(pin); }
static inline void sage_gpio_pull_up(int pin)         { gpio_pull_up(pin); }
static inline void sage_gpio_pull_down(int pin)       { gpio_pull_down(pin); }

#define SAGE_GPIO_FUNC_UART  GPIO_FUNC_UART
#define SAGE_GPIO_FUNC_I2C   GPIO_FUNC_I2C
#define SAGE_GPIO_FUNC_SPI   GPIO_FUNC_SPI
#define SAGE_GPIO_FUNC_PWM   GPIO_FUNC_PWM
#define SAGE_GPIO_FUNC_SIO   GPIO_FUNC_SIO

/* ================================================================
   UART (uart0 on pins 0/1)
   ================================================================ */
static inline void sage_uart_init(int baud) {
    uart_init(uart0, baud);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
}
static inline void sage_uart_putc(char c)            { uart_putc(uart0, c); }
static inline void sage_uart_puts(const char *s)     { uart_puts(uart0, s); }
static inline char sage_uart_getc(void)              { return uart_getc(uart0); }
static inline bool sage_uart_is_readable(void)       { return uart_is_readable(uart0); }

/* ================================================================
   Time
   ================================================================ */
static inline void     sage_sleep_ms(int ms)         { sleep_ms(ms); }
static inline void     sage_sleep_us(int us)         { sleep_us(us); }
static inline uint64_t sage_time_us(void)            { return time_us_64(); }
static inline uint32_t sage_time_ms(void)            { return to_ms_since_boot(get_absolute_time()); }

/* ================================================================
   Print
   ================================================================ */
#define sage_print(s) printf("%s\n", s)

#endif
