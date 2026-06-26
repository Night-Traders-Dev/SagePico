/* SageNative Bridge: pico_port functions wrapped for Sage C backend.
   Each function takes/returns SageValue and follows the sage_native_* pattern.
   Included into generated hello.c by patch_main.py.

   Usage from Sage:
     import pico
     pico.gpio_put(25, 1)
     pico.sleep_ms(500)
     pico.gpio_put(25, 0)
*/

/* ---- helpers ---- */
static inline int sv_int(SageValue v) {
    if (v.type == SAGE_TAG_NUMBER) return (int)v.as.number;
    if (v.type == SAGE_TAG_BOOL)   return v.as.boolean;
    return 0;
}
static inline double sv_num(SageValue v) {
    if (v.type == SAGE_TAG_NUMBER) return v.as.number;
    return 0.0;
}
static inline const char* sv_str(SageValue v) {
    if (v.type == SAGE_TAG_STRING) return v.as.string;
    return "";
}

/* ---- GPIO (6 functions) ---- */
static SageValue sage_nv_gpio_init(SageValue pin) {
    gpio_init(sv_int(pin));
    return sage_nil();
}
static SageValue sage_nv_gpio_set_dir(SageValue pin, SageValue dir) {
    gpio_set_dir(sv_int(pin), sv_int(dir));
    return sage_nil();
}
static SageValue sage_nv_gpio_put(SageValue pin, SageValue val) {
    gpio_put(sv_int(pin), sv_int(val));
    return sage_nil();
}
static SageValue sage_nv_gpio_get(SageValue pin) {
    return sage_number((double)gpio_get(sv_int(pin)));
}
static SageValue sage_nv_gpio_pull_up(SageValue pin) {
    gpio_pull_up(sv_int(pin));
    return sage_nil();
}
static SageValue sage_nv_gpio_pull_down(SageValue pin) {
    gpio_pull_down(sv_int(pin));
    return sage_nil();
}
static SageValue sage_nv_gpio_set_function(SageValue pin, SageValue func) {
    gpio_set_function(sv_int(pin), sv_int(func));
    return sage_nil();
}

/* ---- Time (4 functions) ---- */
static SageValue sage_nv_sleep_ms(SageValue ms) {
    sleep_ms(sv_int(ms));
    return sage_nil();
}
static SageValue sage_nv_sleep_us(SageValue us) {
    sleep_us(sv_int(us));
    return sage_nil();
}
static SageValue sage_nv_time_us(SageValue _) {
    (void)_;
    return sage_number((double)time_us_64());
}
static SageValue sage_nv_time_ms(SageValue _) {
    (void)_;
    return sage_number((double)to_ms_since_boot(get_absolute_time()));
}

/* ---- UART (5 functions) ---- */
static SageValue sage_nv_uart_init(SageValue baud) {
    uart_init(uart0, sv_int(baud));
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    return sage_nil();
}
static SageValue sage_nv_uart_putc(SageValue c) {
    uart_putc(uart0, (char)sv_int(c));
    return sage_nil();
}
static SageValue sage_nv_uart_puts(SageValue s) {
    uart_puts(uart0, sv_str(s));
    return sage_nil();
}
static SageValue sage_nv_uart_getc(SageValue _) {
    (void)_;
    return sage_number((double)(unsigned char)uart_getc(uart0));
}
static SageValue sage_nv_uart_readable(SageValue _) {
    (void)_;
    return sage_bool(uart_is_readable(uart0));
}

/* ---- ADC (4 functions) ---- */
static SageValue sage_nv_adc_init(SageValue _) {
    (void)_; adc_init();
    return sage_nil();
}
static SageValue sage_nv_adc_gpio(SageValue pin) {
    adc_gpio_init(sv_int(pin));
    return sage_nil();
}
static SageValue sage_nv_adc_select(SageValue ch) {
    adc_select_input(sv_int(ch));
    return sage_nil();
}
static SageValue sage_nv_adc_read(SageValue _) {
    (void)_;
    return sage_number((double)adc_read());
}

/* ---- PWM (2 functions) ---- */
static SageValue sage_nv_pwm_setup(SageValue pin, SageValue wrap, SageValue level) {
    int p = sv_int(pin);
    gpio_set_function(p, GPIO_FUNC_PWM);
    uint s = pwm_gpio_to_slice_num(p);
    pwm_set_wrap(s, sv_int(wrap));
    pwm_set_chan_level(s, pwm_gpio_to_channel(p), sv_int(wrap) * (uint)sv_num(level));
    pwm_set_enabled(s, true);
    return sage_nil();
}
static SageValue sage_nv_pwm_duty(SageValue pin, SageValue duty) {
    int p = sv_int(pin);
    uint s = pwm_gpio_to_slice_num(p);
    pwm_set_chan_level(s, pwm_gpio_to_channel(p), pwm_hw->slice[s].top * (uint)sv_num(duty));
    return sage_nil();
}

/* ---- I2C (3 functions) ---- */
#include "hardware/i2c.h"
static SageValue sage_nv_i2c_init(SageValue baud) {
    i2c_init(i2c0, sv_int(baud));
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4); gpio_pull_up(5);
    return sage_nil();
}
static SageValue sage_nv_i2c_write(SageValue addr, SageValue data) {
    (void)data;
    return sage_nil();
}
static SageValue sage_nv_i2c_read(SageValue addr, SageValue len) {
    (void)addr; (void)len;
    return sage_nil();
}

/* ---- SPI (2 functions) ---- */
#include "hardware/spi.h"
static SageValue sage_nv_spi_init(SageValue baud) {
    spi_init(spi0, sv_int(baud));
    gpio_set_function(16, GPIO_FUNC_SPI);
    gpio_set_function(17, GPIO_FUNC_SPI);
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    return sage_nil();
}
static SageValue sage_nv_spi_xfer(SageValue txdata) {
    (void)txdata;
    return sage_nil();
}

/* ---- sage_init_native_module override ---- */
static SageValue sage_init_native_module(const char* name) {
    sage_gc_pin();
    if (strcmp(name, "pico") == 0 || strcmp(name, "gpio") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "GPIO_OUT", sage_number(1));
        sage_dict_set(m.as.dict, "GPIO_IN",  sage_number(0));
        sage_dict_set(m.as.dict, "gpio_init",   sage_function((void*)sage_nv_gpio_init));
        sage_dict_set(m.as.dict, "gpio_set_dir", sage_function((void*)sage_nv_gpio_set_dir));
        sage_dict_set(m.as.dict, "gpio_put",     sage_function((void*)sage_nv_gpio_put));
        sage_dict_set(m.as.dict, "gpio_get",     sage_function((void*)sage_nv_gpio_get));
        sage_dict_set(m.as.dict, "gpio_pull_up",  sage_function((void*)sage_nv_gpio_pull_up));
        sage_dict_set(m.as.dict, "gpio_pull_down",sage_function((void*)sage_nv_gpio_pull_down));
        sage_dict_set(m.as.dict, "gpio_set_function", sage_function((void*)sage_nv_gpio_set_function));
        sage_dict_set(m.as.dict, "FUNC_UART", sage_number(GPIO_FUNC_UART));
        sage_dict_set(m.as.dict, "FUNC_I2C",  sage_number(GPIO_FUNC_I2C));
        sage_dict_set(m.as.dict, "FUNC_SPI",  sage_number(GPIO_FUNC_SPI));
        sage_dict_set(m.as.dict, "FUNC_PWM",  sage_number(GPIO_FUNC_PWM));
        sage_dict_set(m.as.dict, "FUNC_SIO",  sage_number(GPIO_FUNC_SIO));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "time") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "sleep_ms", sage_function((void*)sage_nv_sleep_ms));
        sage_dict_set(m.as.dict, "sleep_us", sage_function((void*)sage_nv_sleep_us));
        sage_dict_set(m.as.dict, "time_us",  sage_function((void*)sage_nv_time_us));
        sage_dict_set(m.as.dict, "time_ms",  sage_function((void*)sage_nv_time_ms));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "uart") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "init",     sage_function((void*)sage_nv_uart_init));
        sage_dict_set(m.as.dict, "putc",     sage_function((void*)sage_nv_uart_putc));
        sage_dict_set(m.as.dict, "puts",     sage_function((void*)sage_nv_uart_puts));
        sage_dict_set(m.as.dict, "getc",     sage_function((void*)sage_nv_uart_getc));
        sage_dict_set(m.as.dict, "readable", sage_function((void*)sage_nv_uart_readable));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "adc") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "init",   sage_function((void*)sage_nv_adc_init));
        sage_dict_set(m.as.dict, "gpio_init", sage_function((void*)sage_nv_adc_gpio));
        sage_dict_set(m.as.dict, "select", sage_function((void*)sage_nv_adc_select));
        sage_dict_set(m.as.dict, "read",   sage_function((void*)sage_nv_adc_read));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "pwm") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "setup", sage_function((void*)sage_nv_pwm_setup));
        sage_dict_set(m.as.dict, "duty",  sage_function((void*)sage_nv_pwm_duty));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "i2c") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "init",  sage_function((void*)sage_nv_i2c_init));
        sage_dict_set(m.as.dict, "write", sage_function((void*)sage_nv_i2c_write));
        sage_dict_set(m.as.dict, "read",  sage_function((void*)sage_nv_i2c_read));
        sage_gc_unpin();
        return m;
    }
    if (strcmp(name, "spi") == 0) {
        SageValue m = sage_make_dict();
        sage_dict_set(m.as.dict, "init",  sage_function((void*)sage_nv_spi_init));
        sage_dict_set(m.as.dict, "transfer", sage_function((void*)sage_nv_spi_xfer));
        sage_gc_unpin();
        return m;
    }
    sage_gc_unpin();
    /* fallback: math, sys, io etc. — return empty dicts */
    return sage_make_dict();
}

/* ---- FFI: ffi_open / ffi_call for baremetal (replaces dlopen stubs) ---- */

/* Known library handles: small integers, not real pointers */
#define FFI_HANDLE_PICO  ((void*)1)
#define FFI_HANDLE_GPIO  ((void*)2)
#define FFI_HANDLE_TIME  ((void*)3)
#define FFI_HANDLE_UART  ((void*)4)
#define FFI_HANDLE_ADC   ((void*)5)
#define FFI_HANDLE_PWM   ((void*)6)
#define FFI_HANDLE_I2C   ((void*)7)
#define FFI_HANDLE_SPI   ((void*)8)

static SageValue sage_ffi_open(SageValue libname) {
    if (libname.type != SAGE_TAG_STRING) return sage_nil();
    const char *s = libname.as.string;
    void *h = NULL;
    if (strcmp(s, "pico") == 0 || strcmp(s, "gpio") == 0) h = FFI_HANDLE_PICO;
    else if (strcmp(s, "time") == 0) h = FFI_HANDLE_TIME;
    else if (strcmp(s, "uart") == 0) h = FFI_HANDLE_UART;
    else if (strcmp(s, "adc") == 0)  h = FFI_HANDLE_ADC;
    else if (strcmp(s, "pwm") == 0)  h = FFI_HANDLE_PWM;
    else if (strcmp(s, "i2c") == 0)  h = FFI_HANDLE_I2C;
    else if (strcmp(s, "spi") == 0)  h = FFI_HANDLE_SPI;
    if (!h) return sage_nil();
    SageValue v; v.type = SAGE_TAG_CLIB; v.as.clib = h; return v;
}

static SageValue sage_ffi_close(SageValue handle) {
    (void)handle;
    return sage_nil();
}

/* Typedef for a native function that takes an array of SageValue args */
typedef SageValue (*SageNativeFn)(int argc, SageValue* argv);

/* FFI dispatch table entry */
typedef struct {
    void*       handle;
    const char* name;
    void*       fn;        /* SageNativeFn but stored as void* */
} SageFFIEntry;

/* GPIO 0-arg */
static SageValue sage_ffi_gpio_get_wrap(int argc, SageValue* argv) {
    (void)argc; return sage_nv_gpio_get(argv[0]);
}
/* GPIO 1-arg (no return) */
static SageValue sage_ffi_gpio_init_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_init(argv[0]); return sage_nil();
}
static SageValue sage_ffi_gpio_pull_up_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_pull_up(argv[0]); return sage_nil();
}
static SageValue sage_ffi_gpio_pull_down_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_pull_down(argv[0]); return sage_nil();
}
/* GPIO 2-arg */
static SageValue sage_ffi_gpio_set_dir_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_set_dir(argv[0], argv[1]); return sage_nil();
}
static SageValue sage_ffi_gpio_put_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_put(argv[0], argv[1]); return sage_nil();
}
static SageValue sage_ffi_gpio_set_func_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_gpio_set_function(argv[0], argv[1]); return sage_nil();
}
/* Time */
static SageValue sage_ffi_sleep_ms_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_sleep_ms(argv[0]); return sage_nil();
}
static SageValue sage_ffi_sleep_us_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_sleep_us(argv[0]); return sage_nil();
}
static SageValue sage_ffi_time_us_wrap(int argc, SageValue* argv) {
    (void)argc; (void)argv; return sage_nv_time_us(sage_nil());
}
/* UART */
static SageValue sage_ffi_uart_putc_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_uart_putc(argv[0]); return sage_nil();
}
static SageValue sage_ffi_uart_puts_wrap(int argc, SageValue* argv) {
    (void)argc; sage_nv_uart_puts(argv[0]); return sage_nil();
}

#define FFI_ENTRY(handle, name, fn) { (void*)(handle), name, (void*)(fn) }

static const SageFFIEntry sage_ffi_table[] = {
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_init",    sage_ffi_gpio_init_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_set_dir", sage_ffi_gpio_set_dir_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_put",     sage_ffi_gpio_put_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_get",     sage_ffi_gpio_get_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_pull_up",   sage_ffi_gpio_pull_up_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_pull_down", sage_ffi_gpio_pull_down_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_set_function", sage_ffi_gpio_set_func_wrap),
    FFI_ENTRY(FFI_HANDLE_TIME, "sleep_ms", sage_ffi_sleep_ms_wrap),
    FFI_ENTRY(FFI_HANDLE_TIME, "sleep_us", sage_ffi_sleep_us_wrap),
    FFI_ENTRY(FFI_HANDLE_TIME, "time_us",  sage_ffi_time_us_wrap),
    FFI_ENTRY(FFI_HANDLE_UART, "putc", sage_ffi_uart_putc_wrap),
    FFI_ENTRY(FFI_HANDLE_UART, "puts", sage_ffi_uart_puts_wrap),
};
#define SAGE_FFI_TABLE_LEN (sizeof(sage_ffi_table) / sizeof(sage_ffi_table[0]))

static SageValue sage_ffi_call(SageValue handle, SageValue name, SageValue args) {
    if (handle.type != SAGE_TAG_CLIB || name.type != SAGE_TAG_STRING) return sage_nil();
    void* h = handle.as.clib;
    const char* fn_name = name.as.string;

    /* Extract args from array */
    int argc = 0;
    SageValue argv[8];
    if (args.type == SAGE_TAG_ARRAY) {
        argc = args.as.array->count;
        if (argc > 8) argc = 8;
        for (int i = 0; i < argc; i++)
            argv[i] = args.as.array->elements[i];
    }

    /* Lookup and call */
    for (size_t i = 0; i < SAGE_FFI_TABLE_LEN; i++) {
        if (sage_ffi_table[i].handle == h &&
            strcmp(sage_ffi_table[i].name, fn_name) == 0) {
            SageNativeFn fn = (SageNativeFn)sage_ffi_table[i].fn;
            return fn(argc, argv);
        }
    }
    return sage_nil();
}

static SageValue sage_ffi_call_full(SageValue handle, SageValue name, SageValue args, SageValue rt) {
    (void)rt;
    return sage_ffi_call(handle, name, args);
}
