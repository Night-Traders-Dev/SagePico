/* SageNative Bridge: pico_port functions wrapped for Sage C backend.
   Each function takes/returns SageValue and follows the sage_native_* pattern.
   Included into generated hello.c by patch_main.py.

   Usage from Sage:
     import pico
     pico.gpio_put(25, 1)
     pico.sleep_ms(500)
     pico.gpio_put(25, 0)
*/

/* Forward decls from flash_store.h (injected before us) */
static void flash_store_init(void);
static int  flash_store_put(const char* key, const uint8_t* val, uint16_t vlen);
static int  flash_store_get(const char* key, uint8_t* val_out, uint16_t* vlen_out);
static int  flash_store_del(const char* key);
static void flash_store_keys(char keys[][64], int* count);
static void flash_persist_repl_vars(void);

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
/* Flash store */
static SageValue sage_ffi_flash_save_wrap(int argc, SageValue* argv) {
    if (argc >= 2 && argv[0].type == SAGE_TAG_STRING && argv[1].type == SAGE_TAG_STRING) {
        flash_store_init();
        flash_store_put(argv[0].as.string,
                       (const uint8_t*)argv[1].as.string,
                       (uint16_t)strlen(argv[1].as.string));
    }
    return sage_nil();
}
static SageValue sage_ffi_flash_load_wrap(int argc, SageValue* argv) {
    if (argc >= 1 && argv[0].type == SAGE_TAG_STRING) {
        uint8_t buf[256]; uint16_t len = 0;
        if (flash_store_get(argv[0].as.string, buf, &len) == 0) {
            buf[len] = 0;
            return sage_string((const char*)buf);
        }
    }
    return sage_nil();
}
static SageValue sage_ffi_flash_del_wrap(int argc, SageValue* argv) {
    if (argc >= 1 && argv[0].type == SAGE_TAG_STRING)
        flash_store_del(argv[0].as.string);
    return sage_nil();
}
static SageValue sage_ffi_flash_keys_wrap(int argc, SageValue* argv) {
    (void)argc; (void)argv;
    char keys[64][64]; int n = 0;
    flash_store_keys(keys, &n);
    SageValue arr = sage_array();
    for (int i = 0; i < n; i++)
        sage_array_push_raw(arr.as.array, sage_string(keys[i]));
    return arr;
}
/* PIO wrappers */
static SageValue sage_ffi_pio_claim_wrap(int argc, SageValue* argv) {
    (void)argc;
    return sage_bool(sage_pio_claim(sv_int(argv[0]), sv_int(argv[1])));
}
static SageValue sage_ffi_pio_put_wrap(int argc, SageValue* argv) {
    sage_pio_sm_put(sv_int(argv[0]), sv_int(argv[1]), (uint32_t)sv_int(argv[2]));
    return sage_nil();
}
static SageValue sage_ffi_pio_get_wrap(int argc, SageValue* argv) {
    (void)argc;
    return sage_number((double)sage_pio_sm_get(sv_int(argv[0]), sv_int(argv[1])));
}
static SageValue sage_ffi_pio_set_pins_wrap(int argc, SageValue* argv) {
    sage_pio_sm_set_pins(sv_int(argv[0]), sv_int(argv[1]), sv_int(argv[2]), sv_int(argv[3]), sv_int(argv[4]));
    return sage_nil();
}
static SageValue sage_ffi_pio_set_enabled_wrap(int argc, SageValue* argv) {
    sage_pio_sm_set_enabled(sv_int(argv[0]), sv_int(argv[1]), sv_int(argv[2]));
    return sage_nil();
}
static SageValue sage_ffi_pio_set_clkdiv_wrap(int argc, SageValue* argv) {
    (void)argc;
    sage_pio_sm_set_clkdiv(sv_int(argv[0]), sv_int(argv[1]), (float)sv_num(argv[2]));
    return sage_nil();
}
static SageValue sage_ffi_pio_clear_fifos_wrap(int argc, SageValue* argv) {
    sage_pio_sm_clear_fifos(sv_int(argv[0]), sv_int(argv[1]));
    return sage_nil();
}
static SageValue sage_ffi_ws2812_init_wrap(int argc, SageValue* argv) {
    (void)argc;
    return sage_number((double)sage_ws2812_init(sv_int(argv[0]), sv_int(argv[1])));
}
static SageValue sage_ffi_ws2812_put_wrap(int argc, SageValue* argv) {
    sage_ws2812_put(sv_int(argv[0]), sv_int(argv[1]),
                   (uint8_t)sv_int(argv[2]), (uint8_t)sv_int(argv[3]), (uint8_t)sv_int(argv[4]));
    return sage_nil();
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
    FFI_ENTRY(FFI_HANDLE_PICO, "flash_save",  sage_ffi_flash_save_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "flash_load",  sage_ffi_flash_load_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "flash_del",   sage_ffi_flash_del_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "flash_keys",  sage_ffi_flash_keys_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_claim",     sage_ffi_pio_claim_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_put",       sage_ffi_pio_put_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_get",       sage_ffi_pio_get_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_set_pins",  sage_ffi_pio_set_pins_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_set_enabled", sage_ffi_pio_set_enabled_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_set_clkdiv", sage_ffi_pio_set_clkdiv_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "pio_clear_fifos", sage_ffi_pio_clear_fifos_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "ws2812_init",   sage_ffi_ws2812_init_wrap),
    FFI_ENTRY(FFI_HANDLE_PICO, "ws2812_put",    sage_ffi_ws2812_put_wrap),
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

/* ---- Mini REPL: expression parser + command shell ---- */

static SageValue sage_repl_vars;  /* dict: variable name -> value */

/* Forward decls */
static SageValue sage_repl_parse_expr(const char* s, int* pos);
static void sage_repl_init(void) {
    sage_repl_vars = sage_make_dict();
}

/* Skip whitespace */
static void sage_repl_skip_ws(const char* s, int* pos) {
    while (s[*pos] == ' ' || s[*pos] == '\t' || s[*pos] == '\n' || s[*pos] == '\r')
        (*pos)++;
}

/* Parse a number literal */
static SageValue sage_repl_parse_number(const char* s, int* pos) {
    int start = *pos;
    if (s[*pos] == '-') (*pos)++;
    while (s[*pos] >= '0' && s[*pos] <= '9') (*pos)++;
    if (s[*pos] == '.') { (*pos)++; while (s[*pos] >= '0' && s[*pos] <= '9') (*pos)++; }
    if (*pos == start) return sage_nil();
    char buf[64]; int len = *pos - start;
    if (len > 63) len = 63;
    memcpy(buf, s + start, len); buf[len] = 0;
    return sage_number(atof(buf));
}

/* Parse a string literal */
static SageValue sage_repl_parse_string(const char* s, int* pos) {
    if (s[*pos] != '"') return sage_nil();
    (*pos)++; int start = *pos;
    while (s[*pos] && s[*pos] != '"') (*pos)++;
    int len = *pos - start;
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, s + start, len); buf[len] = 0;
    if (s[*pos] == '"') (*pos)++;
    SageValue v = sage_string_take(buf);
    return v;
}

/* Parse an identifier */
static int sage_repl_parse_ident(const char* s, int* pos, char* out, int max) {
    int start = *pos;
    while ((s[*pos] >= 'a' && s[*pos] <= 'z') ||
           (s[*pos] >= 'A' && s[*pos] <= 'Z') ||
           (s[*pos] >= '0' && s[*pos] <= '9') ||
           s[*pos] == '_')
        (*pos)++;
    int len = *pos - start;
    if (len == 0 || len >= max) return 0;
    memcpy(out, s + start, len); out[len] = 0;
    return 1;
}

/* Parse a primary expression: number, string, identifier, (expr), [expr,...] */
static SageValue sage_repl_parse_primary(const char* s, int* pos) {
    sage_repl_skip_ws(s, pos);
    if (s[*pos] == 0) return sage_nil();

    /* Number */
    if ((s[*pos] >= '0' && s[*pos] <= '9') || (s[*pos] == '-' && s[*pos+1] >= '0' && s[*pos+1] <= '9'))
        return sage_repl_parse_number(s, pos);

    /* String */
    if (s[*pos] == '"')
        return sage_repl_parse_string(s, pos);

    /* nil / true / false */
    if (strncmp(s + *pos, "nil", 3) == 0 && !isalnum((unsigned char)s[*pos+3]) && s[*pos+3] != '_')
        { *pos += 3; return sage_nil(); }
    if (strncmp(s + *pos, "true", 4) == 0 && !isalnum((unsigned char)s[*pos+4]) && s[*pos+4] != '_')
        { *pos += 4; return sage_bool(1); }
    if (strncmp(s + *pos, "false", 5) == 0 && !isalnum((unsigned char)s[*pos+5]) && s[*pos+5] != '_')
        { *pos += 5; return sage_bool(0); }

    /* Array literal [a, b, c] */
    if (s[*pos] == '[') {
        (*pos)++;
        SageValue arr = sage_array();
        while (1) {
            sage_repl_skip_ws(s, pos);
            if (s[*pos] == ']') { (*pos)++; break; }
            SageValue elem = sage_repl_parse_expr(s, pos);
            if (elem.type != SAGE_TAG_NIL || s[*pos] != ',') {
                sage_array_push_raw(arr.as.array, elem);
            }
            sage_repl_skip_ws(s, pos);
            if (s[*pos] == ',') (*pos)++;
            else if (s[*pos] == ']') { (*pos)++; break; }
        }
        return arr;
    }

    /* Parenthesized expression */
    if (s[*pos] == '(') {
        (*pos)++;
        SageValue v = sage_repl_parse_expr(s, pos);
        sage_repl_skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        return v;
    }

    /* Identifier: variable lookup */
    char ident[64];
    int saved = *pos;
    if (sage_repl_parse_ident(s, pos, ident, sizeof(ident))) {
        return sage_dict_get(sage_repl_vars.as.dict, ident);
    }
    *pos = saved;
    return sage_nil();
}

/* Parse binary operator expression (precedence climbing) */
static SageValue sage_repl_parse_binary(const char* s, int* pos, int min_prec);

static int sage_repl_op_prec(const char* op) {
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 1;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) return 2;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 0;
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) return 0;
    return -1;
}

/* Parse a function call or identifier */
static SageValue sage_repl_parse_call(const char* s, int* pos) {
    sage_repl_skip_ws(s, pos);
    int saved = *pos;
    char ident[64];
    if (!sage_repl_parse_ident(s, pos, ident, sizeof(ident))) {
        *pos = saved;
        return sage_repl_parse_primary(s, pos);
    }

    sage_repl_skip_ws(s, pos);
    if (s[*pos] != '(') {
        /* Variable reference */
        if (s[*pos] == '=') {
            /* Assignment: identifier = expr */
            (*pos)++;
            SageValue rhs = sage_repl_parse_expr(s, pos);
            sage_dict_set(sage_repl_vars.as.dict, ident, rhs);
            return rhs;
        }
        /* Just a variable lookup */
        SageValue v = sage_dict_get(sage_repl_vars.as.dict, ident);
        if (v.type == SAGE_TAG_NIL) {
            /* Check built-in FFI: zero-arg call */
            for (size_t i = 0; i < SAGE_FFI_TABLE_LEN; i++) {
                if (strcmp(sage_ffi_table[i].name, ident) == 0) {
                    SageNativeFn fn = (SageNativeFn)sage_ffi_table[i].fn;
                    return fn(0, NULL);
                }
            }
        }
        return v;
    }

    /* Function call: parse arguments */
    (*pos)++; /* skip '(' */
    SageValue args_arr = sage_array();
    while (1) {
        sage_repl_skip_ws(s, pos);
        if (s[*pos] == ')') { (*pos)++; break; }
        SageValue arg = sage_repl_parse_expr(s, pos);
        sage_array_push_raw(args_arr.as.array, arg);
        sage_repl_skip_ws(s, pos);
        if (s[*pos] == ',') (*pos)++;
        else if (s[*pos] == ')') { (*pos)++; break; }
    }

    /* Try FFI dispatch first (built-in functions) */
    for (size_t i = 0; i < SAGE_FFI_TABLE_LEN; i++) {
        if (strcmp(sage_ffi_table[i].name, ident) == 0) {
            SageNativeFn fn = (SageNativeFn)sage_ffi_table[i].fn;
            SageValue cargv[8];
            int cargc = args_arr.as.array->count;
            if (cargc > 8) cargc = 8;
            for (int j = 0; j < cargc; j++)
                cargv[j] = args_arr.as.array->elements[j];
            return fn(cargc, cargv);
        }
    }

    /* Try as user-defined variable containing a function */
    SageValue fn_val = sage_dict_get(sage_repl_vars.as.dict, ident);
    (void)fn_val;
    return sage_nil();
}

/* Binary expression parser with precedence climbing */
static SageValue sage_repl_parse_binary(const char* s, int* pos, int min_prec) {
    SageValue left = sage_repl_parse_call(s, pos);

    while (1) {
        sage_repl_skip_ws(s, pos);
        char op[3] = {0};
        int saved = *pos;
        if (s[*pos] == '+' || s[*pos] == '-' || s[*pos] == '*' || s[*pos] == '/' || s[*pos] == '%' ||
            s[*pos] == '<' || s[*pos] == '>') {
            op[0] = s[*pos]; (*pos)++;
            if (s[*pos] == '=') { op[1] = '='; (*pos)++; }
        } else if (s[*pos] == '!' && s[*pos+1] == '=') {
            op[0] = '!'; op[1] = '='; *pos += 2;
        } else if (s[*pos] == '=' && s[*pos+1] == '=') {
            op[0] = '='; op[1] = '='; *pos += 2;
        } else {
            break;
        }

        int prec = sage_repl_op_prec(op);
        if (prec < min_prec) {
            *pos = saved;
            break;
        }

        SageValue right = sage_repl_parse_binary(s, pos, prec + 1);

        /* Evaluate */
        if (left.type == SAGE_TAG_NUMBER && right.type == SAGE_TAG_NUMBER) {
            double a = left.as.number, b = right.as.number;
            if      (strcmp(op, "+") == 0) left = sage_number(a + b);
            else if (strcmp(op, "-") == 0) left = sage_number(a - b);
            else if (strcmp(op, "*") == 0) left = sage_number(a * b);
            else if (strcmp(op, "/") == 0) left = sage_number(b != 0 ? a / b : 0);
            else if (strcmp(op, "%") == 0) left = sage_number(b != 0 ? fmod(a, b) : 0);
            else if (strcmp(op, "==") == 0) left = sage_bool(a == b);
            else if (strcmp(op, "!=") == 0) left = sage_bool(a != b);
            else if (strcmp(op, "<") == 0) left = sage_bool(a < b);
            else if (strcmp(op, ">") == 0) left = sage_bool(a > b);
            else if (strcmp(op, "<=") == 0) left = sage_bool(a <= b);
            else if (strcmp(op, ">=") == 0) left = sage_bool(a >= b);
            else break;
        } else if (left.type == SAGE_TAG_STRING && right.type == SAGE_TAG_STRING && strcmp(op, "+") == 0) {
            size_t l1 = strlen(left.as.string), l2 = strlen(right.as.string);
            char* r = (char*)malloc(l1 + l2 + 1);
            memcpy(r, left.as.string, l1); memcpy(r + l1, right.as.string, l2 + 1);
            left = sage_string_take(r);
        } else {
            break;
        }
    }
    return left;
}

/* Top-level expression entry */
static SageValue sage_repl_parse_expr(const char* s, int* pos) {
    sage_repl_skip_ws(s, pos);
    if (s[*pos] == 0) return sage_nil();
    return sage_repl_parse_binary(s, pos, 0);
}

/* Parse and execute a line of input */
static void sage_repl_eval(const char* cmd) {
    int pos = 0;
    sage_repl_skip_ws(cmd, &pos);
    if (cmd[pos] == 0 || cmd[pos] == '#') return;

    if (strncmp(cmd + pos, "let ", 4) == 0) {
        pos += 4;
        sage_repl_skip_ws(cmd, &pos);
        char ident[64];
        if (sage_repl_parse_ident(cmd, &pos, ident, sizeof(ident))) {
            sage_repl_skip_ws(cmd, &pos);
            if (cmd[pos] == '=') {
                pos++;
                SageValue v = sage_repl_parse_expr(cmd, &pos);
                sage_dict_set(sage_repl_vars.as.dict, ident, v);
                printf("  %s = ", ident);
                con_printf("  %s = ", ident);
                sage_print_value(v);
                printf("\n");
                con_putchar_raw('\n');
                return;
            }
        }
    }

    /* Try as expression */
    SageValue result = sage_repl_parse_expr(cmd, &pos);
    if (result.type != SAGE_TAG_NIL) {
        sage_print_value(result);
        printf("\n");
        con_putchar_raw('\n');
    }
}

/* Mini REPL using printf/scanf over USB CDC */
static void sage_repl(void) {
    printf("\n=== Sage REPL ===\n");
    con_printf("\n=== Sage REPL ===\n");
    printf("Type expressions: 2+2, gpio_put(7,1), let x=42, x*2\n");
    con_puts("Type expressions: 2+2, gpio_put(7,1), let x=42, x*2\n");
    printf("Builtins: gpio_init, gpio_set_dir, gpio_put, gpio_get, gpio_pull_up,\n");
    con_puts("Builtins: gpio_init, gpio_set_dir, gpio_put, gpio_get, gpio_pull_up,\n");
    printf("          gpio_pull_down, gpio_set_function, sleep_ms, sleep_us, time_us\n");
    con_puts("          gpio_pull_down, gpio_set_function, sleep_ms, sleep_us, time_us\n");
    printf("Ctrl+C to exit REPL, resume display\n\n");
    con_puts("Ctrl+C to exit REPL, resume display\n\n");

    sage_repl_init();
    flash_store_init();

    /* Restore saved variables */
    {
        char keys[64][64]; int n = 0;
        flash_store_keys(keys, &n);
        if (n > 0) {
            printf("Restored %d vars from flash\n", n);
            con_printf("Restored %d vars from flash\n", n);
            for (int i = 0; i < n; i++) {
                uint8_t buf[256]; uint16_t len = 0;
                if (flash_store_get(keys[i], buf, &len) == 0) {
                    buf[len] = 0;
                    sage_dict_set(sage_repl_vars.as.dict, keys[i],
                                 sage_string((const char*)buf));
                }
            }
        }
    }

    char line[256];
    int idx = 0;
    int repl_active = 1;

    while (repl_active) {
        printf(">>> ");
        con_puts(">>> ");
        idx = 0;
        while (1) {
            int c = getchar_timeout_us(100000); /* 100ms timeout for LED heartbeat */
            if (c == PICO_ERROR_TIMEOUT) continue;
            if (c == '\r' || c == '\n') {
                if (idx > 0) { line[idx] = 0; printf("\n"); con_putchar_raw('\n'); break; }
            } else if (c == 0x03) { /* Ctrl+C */
                printf("^C\nREPL exit, saving vars...\n");
                con_puts("^C\nREPL exit, saving vars...\n");
                flash_persist_repl_vars();
                printf("Resuming display\n");
                con_puts("Resuming display\n");
                repl_active = 0;
                break;
            } else if (c == 0x08 || c == 0x7f) { /* Backspace */
                if (idx > 0) { idx--; printf("\b \b"); }
            } else if (idx < 250 && c >= 32 && c < 127) {
                line[idx++] = (char)c;
                putchar(c);
                con_putchar_raw((char)c);
            }
        }
        if (!repl_active) break;
        if (idx == 0) continue;
        line[idx] = 0;

        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            printf("Saving vars to flash...\n");
            con_puts("Saving vars to flash...\n");
            flash_persist_repl_vars();
            break;
        }
        sage_repl_eval(line);
    }
}

/* Persist REPL variables to flash (called on quit/Ctrl+C) */
static void flash_persist_repl_vars(void) {
    if (sage_repl_vars.type != 5) return; /* SAGE_TAG_DICT */
    SageDict* dict = sage_repl_vars.as.dict;
    char valbuf[64];
    for (int i = 0; i < dict->count; i++) {
        if (!dict->keys[i] || !dict->keys[i][0]) continue;
        SageValue* v = &dict->values[i];
        valbuf[0] = 0;
        if (v->type == 1)      /* SAGE_TAG_NUMBER */
            snprintf(valbuf, sizeof(valbuf), "%g", v->as.number);
        else if (v->type == 2) /* SAGE_TAG_BOOL */
            snprintf(valbuf, sizeof(valbuf), "%s", v->as.boolean ? "true" : "false");
        else if (v->type == 3) /* SAGE_TAG_STRING */
            snprintf(valbuf, sizeof(valbuf), "%s", v->as.string);
        else
            snprintf(valbuf, sizeof(valbuf), "[type=%d]", v->type);
        flash_store_put(dict->keys[i],
                       (const uint8_t*)valbuf,
                       (uint16_t)strlen(valbuf));
    }
}
