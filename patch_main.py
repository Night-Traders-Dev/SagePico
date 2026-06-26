#!/usr/bin/env python3
"""Patch Sage main() — Native bridge + HSTX display driver."""
import re, sys, os

filepath = sys.argv[1]
arch = sys.argv[2] if len(sys.argv) > 2 else "riscv"

with open(filepath, 'r') as f:
    data = f.read()

# 1. Sage print -> printf + remove GC
data = re.sub(r'sage_print_ln\(sage_string\("([^"]*)"\)\)', r'printf("\1\\n")', data)
data = re.sub(r'    SageGcFrame sage_gc_main_frame;\n    sage_gc_push_frame\(&sage_gc_main_frame, NULL, 0\);\n', '', data)
data = re.sub(r'    sage_gc_pop_frame\(&sage_gc_main_frame\);\n', '', data)
data = re.sub(r'    sage_gc_shutdown\(\);\n', '', data)

# 2. Includes
data = data.replace(
    '#include "pico/stdlib.h"',
    '#include "pico/stdlib.h"\n#include "pico_port.h"\n#include "hstx_display.h"'
)

# 3. Strip old FFI stubs (they use dlopen/dlsym which doesn't work on baremetal)
data = re.sub(
    r'static SageValue sage_ffi_open\(SageValue libname\) \{\s*'
    r'if \(libname\.type != SAGE_TAG_STRING\) return sage_nil\(\);\s*'
    r'void\* handle = dlopen\(libname\.as\.string, RTLD_NOW\);\s*'
    r'if \(!handle\) return sage_nil\(\);\s*'
    r'SageValue v; v\.type = SAGE_TAG_CLIB; v\.as\.clib = handle; return v;\s*'
    r'\}\n',
    '', data, flags=re.DOTALL)
data = re.sub(
    r'static SageValue sage_ffi_close\(SageValue handle\) \{\s*'
    r'if \(handle\.type != SAGE_TAG_CLIB\) return sage_nil\(\);\s*'
    r'dlclose\(handle\.as\.clib\);\s*'
    r'return sage_nil\(\);\s*'
    r'\}\n',
    '', data, flags=re.DOTALL)
data = re.sub(
    r'static SageValue sage_ffi_call\(SageValue handle, SageValue name, SageValue args\) \{ return sage_nil\(\); \}\n',
    '', data)
data = re.sub(
    r'static SageValue sage_ffi_call_full\(SageValue handle, SageValue name, SageValue args, SageValue rt\) \{ return sage_nil\(\); \}\n',
    '', data)

# 4. Inject sage_bridge.h content (replaces sage_init_native_module + FFI stubs)
#    Positioned after all SageValue types are defined (~line 774)
bridge_path = os.path.join(os.path.dirname(__file__), 'src', 'pico', 'sage_bridge.h')
with open(bridge_path, 'r') as bf:
    bridge_code = bf.read()
data = data.replace(
    'static SageValue sage_init_native_module(const char* name) {\n'
    '    /* For now, just return an empty dict; real native modules should be linked */\n'
    '    return sage_make_dict();\n'
    '}\n',
    bridge_code + '\n'
)

# 4. Init with display + pattern
data = data.replace(
    '    stdio_init_all();\n    sleep_ms(2000);',
    '    stdio_init_all();\n'
    '    gpio_init(7); gpio_set_dir(7, GPIO_OUT);\n'
    '    printf("\\n=== SagePico (' + arch + ') HSTX ===\\n");\n'
    '    display_init();\n'
    '    printf("HSTX init OK\\n");\n'
    '    int bw = FB_WIDTH/7;\n'
    '    for (int i=0;i<7;i++) disp_rect(i*bw,0,bw,FB_HEIGHT,i+1);\n'
    '    printf("Pattern drawn\\n");\n'
    '    sleep_ms(1000);'
)

# 5. Scanline render with hardware TMDS encoder
data = re.sub(
    r'(\n    return 0;\n\}\n)',
    r'\n    int _t=0, _scan=0;\n'
    r'    while (1) {\n'
    r'        gpio_put(7,1);\n'
    r'        int v_active=(_scan>=OUT_V_SYNC+OUT_V_BP && _scan<OUT_V_SYNC+OUT_V_BP+OUT_HEIGHT);\n'
    r'        int v_sync=(_scan<OUT_V_SYNC);\n'
    r'        int fy=v_active?(_scan-OUT_V_SYNC-OUT_V_BP)/2:0;\n'
    r'        if(fy>=FB_HEIGHT)fy=FB_HEIGHT-1;\n'
    r'        for(int hx=0;hx<OUT_H_TOTAL;hx+=2){\n'
    r'            int h_active=(hx>=OUT_H_SYNC+OUT_H_BP && hx<OUT_H_SYNC+OUT_H_BP+OUT_WIDTH);\n'
    r'            int h_sync=(hx<OUT_H_SYNC);\n'
    r'            if(v_active&&h_active){\n'
    r'                int fx0=(hx-OUT_H_SYNC-OUT_H_BP)/2,fx1=fx0+1;\n'
    r'                if(fx0<0)fx0=0;if(fx1>=FB_WIDTH)fx1=FB_WIDTH-1;\n'
    r'                uint16_t c0=disp_palette[disp_fb[fy][fx0]];\n'
    r'                uint16_t c1=disp_palette[disp_fb[fy][fx1]];\n'
    r'                hx_push_tmds((c0>>8)&0xf8,(c0>>3)&0xfc,(c0<<3)&0xf8);\n'
    r'                hx_push_tmds((c1>>8)&0xf8,(c1>>3)&0xfc,(c1<<3)&0xf8);\n'
    r'            }else{\n'
    r'                uint8_t ctl=(h_sync?2:0)|(v_sync?1:0);\n'
    r'                hx_push_tmds(ctl,ctl,ctl);\n'
    r'                hx_push_tmds(ctl,ctl,ctl);\n'
    r'            }\n'
    r'        }\n'
    r'        _scan++;if(_scan>=OUT_V_TOTAL)_scan=0;\n'
    r'        gpio_put(7,0);\n'
    r'        if(_t%60==0)printf("Frame %d\\n",_t/60);\n'
    r'        _t++;\n'
    r'    }\n    return 0;\n}\n',
    data)

with open(filepath, 'w') as f:
    f.write(data)
