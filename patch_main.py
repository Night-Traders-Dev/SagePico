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
    f'#include "pico/stdlib.h"\n#include "pico_port.h"\n#include "hstx_display.h"\n#include "pio_bridge.h"\n#include "dma_bridge.h"\n{arch_include}'
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
with open(os.path.join(base, 'src', 'pico', 'sage_bridge.h')) as f: bridge_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'flash_store.h')) as f: flash_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'rvvm.h')) as f: rvvm_code = f.read()
with open(os.path.join(base, 'src', 'pico', 'gfx_vm.h')) as f: gfxvm_code = f.read()
data = data.replace(
    'static SageValue sage_init_native_module(const char* name) {\n'
    '    /* For now, just return an empty dict; real native modules should be linked */\n'
    '    return sage_make_dict();\n'
    '}\n',
    flash_code + '\n' + rvvm_code + '\n' + gfxvm_code + '\n' + bridge_code + '\n'
)

# 5. Init sequence using arch-specific helpers
data = data.replace(
    '    stdio_init_all();\n    sleep_ms(2000);',
    '    sage_arch_pre_init();\n'
    '    stdio_init_all();\n'
    '    sage_arch_init();\n'
    '    sage_arch_post_init();\n'
    f'    printf("\\n=== SagePico ({arch}) HSTX ===\\n");\n'
    '    display_init();\n'
    '    printf("HSTX init OK\\n");\n'
    '    con_clear();\n'
    f'    con_printf("=== SagePico ({arch}) ===\\n");\n'
    '    con_printf("Display: 1280x800 DVI via HSTX\\n");\n'
    '    int bw = FB_WIDTH/7;\n'
    '    for (int i=0;i<7;i++) disp_rect(i*bw,20,bw,20,i+1);\n'
    '    printf("USB ready, entering REPL in 3s...\\n");\n'
    '    con_printf("USB ready, entering REPL...\\n\\n");\n'
    '    sleep_ms(3000);\n'
    '    printf("Entering REPL...\\n");\n'
    '    sage_repl();'
)

# 6. Scanline render loop — inject after sage_repl(), before Sage program code
data = data.replace(
    '    sage_repl();',
    '    sage_repl();\n'
    '    int _t=0, _scan=0;\n'
    '    while (1) {\n'
    '        gpio_put(7,1);\n'
    '        int v_active=(_scan>=OUT_V_SYNC+OUT_V_BP && _scan<OUT_V_SYNC+OUT_V_BP+OUT_HEIGHT);\n'
    '        int v_sync=(_scan<OUT_V_SYNC);\n'
    '        int fy=v_active?(_scan-OUT_V_SYNC-OUT_V_BP)/2:0;\n'
    '        if(fy>=FB_HEIGHT)fy=FB_HEIGHT-1;\n'
    '        for(int hx=0;hx<OUT_H_TOTAL;hx+=2){\n'
    '            int h_active=(hx>=OUT_H_SYNC+OUT_H_BP && hx<OUT_H_SYNC+OUT_H_BP+OUT_WIDTH);\n'
    '            int h_sync=(hx<OUT_H_SYNC);\n'
    '            if(v_active&&h_active){\n'
    '                int fx0=(hx-OUT_H_SYNC-OUT_H_BP)/2,fx1=fx0+1;\n'
    '                if(fx0<0)fx0=0;if(fx1>=FB_WIDTH)fx1=FB_WIDTH-1;\n'
    '                uint16_t c0=disp_palette[disp_fb[fy*FB_WIDTH+fx0]];\n'
    '                uint16_t c1=disp_palette[disp_fb[fy*FB_WIDTH+fx1]];\n'
    '                hx_push_tmds((c0>>8)&0xf8,(c0>>3)&0xfc,(c0<<3)&0xf8);\n'
    '                hx_push_tmds((c1>>8)&0xf8,(c1>>3)&0xfc,(c1<<3)&0xf8);\n'
    '            }else{\n'
    '                uint8_t ctl=(h_sync?2:0)|(v_sync?1:0);\n'
    '                hx_push_tmds(ctl,ctl,ctl);\n'
    '                hx_push_tmds(ctl,ctl,ctl);\n'
    '            }\n'
    '        }\n'
    '        _scan++;if(_scan>=OUT_V_TOTAL)_scan=0;\n'
    '        gpio_put(7,0);\n'
    '        if(_t%60==0)printf("Frame %d\\n",_t/60);\n'
    '        _t++;\n'
    '    }\n'
)

with open(filepath, 'w') as f:
    f.write(data)
