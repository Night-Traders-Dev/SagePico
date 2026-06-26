#!/usr/bin/env python3
"""Patch generated Sage C main() for baremetal Pico.
   Replaces Sage runtime calls with lightweight pico_port equivalents."""
import re, sys

filepath = sys.argv[1]
arch = sys.argv[2] if len(sys.argv) > 2 else "riscv"

with open(filepath, 'r') as f:
    data = f.read()

# 1. Replace Sage print with pico_port print
data = re.sub(
    r'sage_print_ln\(sage_string\("([^"]*)"\)\)',
    r'printf("\1\\n")',
    data
)

# 2. Remove Sage GC setup/shutdown (not needed without Sage runtime calls)
data = re.sub(
    r'    SageGcFrame sage_gc_main_frame;\n    sage_gc_push_frame\(&sage_gc_main_frame, NULL, 0\);\n',
    '', data)
data = re.sub(
    r'    sage_gc_pop_frame\(&sage_gc_main_frame\);\n',
    '', data)
data = re.sub(
    r'    sage_gc_shutdown\(\);\n',
    '', data)

# 3. Add pico_port.h include
data = data.replace(
    '#include "pico/stdlib.h"',
    '#include "pico/stdlib.h"\n#include "pico_port.h"'
)

# 4. Replace stdio_init_all with extended init + LED + GPIO
data = data.replace(
    '    stdio_init_all();\n    sleep_ms(2000);',
    '    stdio_init_all();\n'
    '    gpio_init(7); gpio_set_dir(7, GPIO_OUT);\n'
    '    printf("\\n=== SagePico (' + arch + ') ===\\n");\n'
    '    sleep_ms(2000);'
)

# 5. Add heartbeat loop before return
data = re.sub(
    r'(\n    return 0;\n\}\n)',
    r'\n    int _t = 0;\n'
    r'    while (1) {\n'
    r'        printf("SagePico uptime %ds\\n", _t++);\n'
    r'        gpio_put(7, 1); sleep_ms(200);\n'
    r'        gpio_put(7, 0); sleep_ms(1800);\n'
    r'    }\n    return 0;\n}\n',
    data)

with open(filepath, 'w') as f:
    f.write(data)
