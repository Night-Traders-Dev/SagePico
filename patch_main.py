#!/usr/bin/env python3
"""Patch generated Sage C main() for baremetal Pico."""
import re, sys

filepath = sys.argv[1]
arch = sys.argv[2] if len(sys.argv) > 2 else "riscv"

with open(filepath, 'r') as f:
    data = f.read()

# 1. Replace sage_print_ln(sage_string("...")) with direct printf
data = re.sub(
    r'sage_print_ln\(sage_string\("([^"]*)"\)\)',
    r'printf("\1\\n")',
    data
)

# 2. Remove Sage GC setup/shutdown (never needed for simple programs)
data = re.sub(
    r'    SageGcFrame sage_gc_main_frame;\n    sage_gc_push_frame\(&sage_gc_main_frame, NULL, 0\);\n',
    '', data)
data = re.sub(
    r'    sage_gc_pop_frame\(&sage_gc_main_frame\);\n',
    '', data)
data = re.sub(
    r'    sage_gc_shutdown\(\);\n',
    '', data)

# 3. Add early printf + LED init before sleep
data = data.replace(
    '    stdio_init_all();\n    sleep_ms(2000);',
    '    stdio_init_all();\n'
    '    gpio_init(7);\n'
    '    gpio_set_dir(7, GPIO_OUT);\n'
    '    printf("\\n=== SagePico (' + arch + ') ===\\n");\n'
    '    sleep_ms(2000);')

# 4. Replace final return with infinite loop + LED heartbeat
data = re.sub(
    r'(\n    return 0;\n\}\n)',
    r'\n    while (1) {\n'
    r'        printf("Hello from Sage on RP2350! [%d]\\n", (int)(time_us_64() / 1000000));\n'
    r'        gpio_put(7, 1); sleep_ms(100);\n'
    r'        gpio_put(7, 0); sleep_ms(100);\n'
    r'        gpio_put(7, 1); sleep_ms(100);\n'
    r'        gpio_put(7, 0); sleep_ms(1900);\n'
    r'    }\n    return 0;\n}\n',
    data)

with open(filepath, 'w') as f:
    f.write(data)
