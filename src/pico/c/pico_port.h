/* Sage-to-Pico SDK bridge - complete peripheral wrappers */
#ifndef SAGE_PICO_PORT_H
#define SAGE_PICO_PORT_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
   GPIO
   ================================================================ */
#define SAGE_GPIO_OUT 1
#define SAGE_GPIO_IN  0

static inline void sage_gpio_init(int p)              { gpio_init(p); }
static inline void sage_gpio_set_dir(int p, int d)    { gpio_set_dir(p, d); }
static inline void sage_gpio_put(int p, int v)        { gpio_put(p, v); }
static inline int  sage_gpio_get(int p)               { return gpio_get(p); }
static inline void sage_gpio_pull_up(int p)           { gpio_pull_up(p); }
static inline void sage_gpio_pull_down(int p)         { gpio_pull_down(p); }
static inline void sage_gpio_set_func(int p, int f)   { gpio_set_function(p, f); }
#define SAGE_FUNC_UART GPIO_FUNC_UART
#define SAGE_FUNC_I2C  GPIO_FUNC_I2C
#define SAGE_FUNC_SPI  GPIO_FUNC_SPI
#define SAGE_FUNC_PWM  GPIO_FUNC_PWM
#define SAGE_FUNC_SIO  GPIO_FUNC_SIO

/* ================================================================
   UART (uart0, pins 0/1)
   ================================================================ */
static inline void sage_uart_init(int b) {
    uart_init(uart0, b); gpio_set_function(0,GPIO_FUNC_UART); gpio_set_function(1,GPIO_FUNC_UART); }
static inline void sage_uart_putc(char c)             { uart_putc(uart0, c); }
static inline void sage_uart_puts(const char *s)      { uart_puts(uart0, s); }
static inline char sage_uart_getc(void)               { return uart_getc(uart0); }
static inline bool sage_uart_readable(void)           { return uart_is_readable(uart0); }

/* ================================================================
   Time
   ================================================================ */
static inline void     sage_sleep_ms(int ms)           { sleep_ms(ms); }
static inline void     sage_sleep_us(int us)           { sleep_us(us); }
static inline uint64_t sage_time_us(void)              { return time_us_64(); }
static inline uint32_t sage_time_ms(void)              { return to_ms_since_boot(get_absolute_time()); }

/* ================================================================
   Print
   ================================================================ */
#define sage_print(s) printf("%s\n", s)

/* ================================================================
   ADC (GPIO 26-29)
   ================================================================ */
static inline void     sage_adc_init(void)             { adc_init(); }
static inline void     sage_adc_gpio(int p)            { adc_gpio_init(p); }
static inline void     sage_adc_select(int ch)         { adc_select_input(ch); }
static inline uint16_t sage_adc_read(void)             { return adc_read(); }

/* ================================================================
   PWM
   ================================================================ */
static inline void sage_pwm_gpio(int pin, uint wrap, float level) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint s = pwm_gpio_to_slice_num(pin);
    uint c = pwm_gpio_to_channel(pin);
    pwm_set_wrap(s, wrap); pwm_set_chan_level(s, c, (uint)(wrap*level)); pwm_set_enabled(s, true); }
static inline void sage_pwm_duty(int pin, float duty) {
    uint s = pwm_gpio_to_slice_num(pin);
    uint c = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(s, c, (uint)(pwm_hw->slice[s].top * duty)); }

/* ================================================================
   I2C (i2c0, pins 4=SDA 5=SCL)
   ================================================================ */
static inline void sage_i2c_init(int b) {
    i2c_init(i2c0, b); gpio_set_function(4,GPIO_FUNC_I2C); gpio_set_function(5,GPIO_FUNC_I2C); gpio_pull_up(4); gpio_pull_up(5); }
static inline int  sage_i2c_write(uint8_t a, const uint8_t *d, size_t n) { return i2c_write_blocking(i2c0,a,d,n,false); }
static inline int  sage_i2c_read(uint8_t a, uint8_t *d, size_t n)        { return i2c_read_blocking(i2c0,a,d,n,false); }

/* ================================================================
   SPI (spi0, pins 16=RX 17=CS 18=SCK 19=TX)
   ================================================================ */
static inline void sage_spi_init(int b) {
    spi_init(spi0, b); gpio_set_function(16,GPIO_FUNC_SPI); gpio_set_function(17,GPIO_FUNC_SPI); gpio_set_function(18,GPIO_FUNC_SPI); gpio_set_function(19,GPIO_FUNC_SPI); }
static inline int  sage_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t n) { return spi_write_read_blocking(spi0,tx,rx,n); }

#endif
