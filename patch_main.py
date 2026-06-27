#!/usr/bin/env python3
"""Patch Sage main() — arch-specific init + native bridge + HSTX display."""
import re, sys, os

filepath = sys.argv[1]
arch = sys.argv[2] if len(sys.argv) > 2 else "arm"

with open(filepath, 'r') as f:
    data = f.read()

# 1. Sage print -> printf + remove GC
data = re.sub(r'sage_print_ln\(sage_string\("([^"]*)"\)\)', r'printf("\1\\n")', data)
data = re.sub(r'    SageGcFrame sage_gc_main_frame;\n    sage_gc_push_frame\(&sage_gc_main_frame, NULL, 0\);\n', '', data)
data = re.sub(r'    sage_gc_pop_frame\(&sage_gc_main_frame\);\n', '', data)
data = re.sub(r'    sage_gc_shutdown\(\);\n', '', data)

# 2. Includes — shared bridges + arch-specific init
arch_include = f'#include "init.h"'  # resolves from src/{arch}/
data = data.replace(
    '#include "pico/stdlib.h"',
    f'#include "pico/stdlib.h"\n#include "pico_port.h"\n#include "pio_bridge.h"\n#include "dma_bridge.h"\n#include "rtc_bridge.h"\n#include "powman_bridge.h"\n#include "interp_bridge.h"\n#include "sha256_bridge.h"\n#include "pio_bitblt.h"\n{arch_include}\n'
    '/* Headless: console stubs (no DVI display) */\n'
    '#define con_printf(...) do{}while(0)\n'
    '#define con_puts(s) do{}while(0)\n'
    '#define con_putchar_raw(c) do{}while(0)\n'
    '#define con_clear() do{}while(0)\n'
    'static uint8_t* disp_fb = NULL;\n'
    '#define FB_WIDTH 640\n'
    '#define FB_HEIGHT 400\n'
)

# 3. Strip generated FFI stubs (all variants from v3.8.7 to v3.9.2)
for pattern in [
    # Old dlopen-based
    r'static SageValue sage_ffi_open\(SageValue libname\) \{\s*'
    r'if \(libname\.type != SAGE_TAG_STRING\) return sage_nil\(\);\s*'
    r'void\* handle = dlopen\(.*?return v;\s*\}\n',
    r'static SageValue sage_ffi_close\(SageValue handle\) \{\s*'
    r'if \(handle\.type != SAGE_TAG_CLIB\) return sage_nil\(\);\s*'
    r'dlclose\(handle\.as\.clib\);\s*return sage_nil\(\);\s*\}\n',
    # 3-arg stubs
    r'static SageValue sage_ffi_call\(SageValue handle, SageValue name, SageValue args\) \{ return sage_nil\(\); \}\n',
    r'static SageValue sage_ffi_call_full\(SageValue handle, SageValue name, SageValue args, SageValue rt\) \{ return sage_nil\(\); \}\n',
    # 4-arg dlopen version (v3.9.0)
    r'static SageValue sage_ffi_call\(SageValue handle, SageValue name, SageValue ret_type, SageValue args\) \{.*?\n    return sage_nil\(\);\n\}\n',
    r'static SageValue sage_ffi_call_full\(SageValue h, SageValue n, SageValue r, SageValue a\) \{ return sage_ffi_call\(h,n,r,a\); \}\n',
    # v3.9.1+ one-liner stubs
    r'static SageValue sage_ffi_open\(SageValue libname\) \{ \(void\)libname; return sage_nil\(\); \}\n',
    r'static SageValue sage_ffi_close\(SageValue handle\) \{ \(void\)handle; return sage_nil\(\); \}\n',
    r'static SageValue sage_ffi_call\(SageValue h, SageValue n, SageValue r, SageValue a\) \{ \(void\)h; \(void\)n; \(void\)r; \(void\)a; return sage_nil\(\); \}\n',
    r'static SageValue sage_ffi_call_full\(SageValue h, SageValue n, SageValue r, SageValue a\) \{ \(void\)h; \(void\)n; \(void\)r; \(void\)a; return sage_nil\(\); \}\n',
]:
    data = re.sub(pattern, '', data, flags=re.DOTALL)

# 4. Inject flash_store.h, rvvm.h, gfx_vm.h, sage_bridge.h
base = os.path.dirname(__file__)
with open(os.path.join(base, 'src', 'pico', 'c', 'sage_bridge.h')) as f: bridge_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'c', 'flash_store.h')) as f: flash_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'c', 'rvvm.h')) as f: rvvm_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'c', 'gfx_vm.h')) as f: gfxvm_code = f.read()
data = data.replace(
    'static SageValue sage_init_native_module(const char* name) {\n'
    '    /* For now, just return an empty dict; real native modules should be linked */\n'
    '    return sage_make_dict();\n'
    '}\n',
    flash_code + '\n' + rvvm_code + '\n' + gfxvm_code + '\n' + bridge_code + '\n'
)

# 5. Init sequence — skip display when no DVI connected
data = data.replace(
    '    stdio_init_all();\n    sleep_ms(2000);',
    '    sage_arch_pre_init();\n'
    '    stdio_init_all();\n'
    '    /* Force LF-only output (no \\r before \\n) */\n'
    '    extern stdio_driver_t stdio_usb;\n'
    '    stdio_set_translate_crlf(&stdio_usb, false);\n'
    '    sage_arch_init();\n'
    '    sage_arch_post_init();\n'
    '    sleep_ms(2000);\n'
    f'    printf("\\n=== SagePico ({arch}) ===\\n");\n'
    '    printf("No DVI — headless mode.\\n");\n'
    '    printf("Entering REPL...\\n\\n");\n'
    '    sage_repl();'
)

# 6. Post-REPL idle loop (headless — no display render)
data = data.replace(
    '    sage_repl();',
    '    sage_repl();\n'
    '    printf("REPL exited — sleeping. Send any key on USB to wake.\\n");\n'
    '    while (1) {\n'
    '        int c = getchar_timeout_us(500000);\n'
    '        if (c != PICO_ERROR_TIMEOUT) {\n'
    '            printf("Re-entering REPL...\\n");\n'
    '            sage_repl();\n'
    '            printf("REPL exited again.\\n");\n'
    '        }\n'
    '        gpio_put(7, !gpio_get(7));\n'
    '    }\n'
)

with open(filepath, 'w') as f:
    f.write(data)
