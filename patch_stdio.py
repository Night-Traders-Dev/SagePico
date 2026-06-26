#!/usr/bin/env python3
"""Patch fputs/fputc -> printf in generated Sage C code for Pico baremetal."""
import sys

with open(sys.argv[1], 'r') as f:
    data = f.read()

# The generated C uses these patterns. Replace them all.
replacements = [
    # fputs to stdout
    ('fputs(value.as.string, stdout)', 'printf("%s", value.as.string)'),
    ('fputs(value.as.boolean ? "true" : "false", stdout)', 'printf("%s", value.as.boolean ? "true" : "false")'),
    ('fputs(", ", stdout)', 'printf(", ")'),
    ('fputs("nil", stdout)', 'printf("nil")'),
    # fputs to stderr
    ('fputs("Unhandled exception: ", stderr)', 'fprintf(stderr, "%s", "Unhandled exception: ")'),
    ('fputs(value.as.string, stderr)', 'fprintf(stderr, "%s", value.as.string)'),
    ('fputs("(unknown)", stderr)', 'fprintf(stderr, "%s", "(unknown)")'),
    ('fputs(message, stderr)', 'fprintf(stderr, "%s", message)'),
    ('fputs("mem_alloc(): expects number\\n", stderr)', 'fprintf(stderr, "%s", "mem_alloc(): expects number\\n")'),
    ('fputs("mem_alloc(): invalid size\\n", stderr)', 'fprintf(stderr, "%s", "mem_alloc(): invalid size\\n")'),
    ('fputs("mem_free(): expects pointer\\n", stderr)', 'fprintf(stderr, "%s", "mem_free(): expects pointer\\n")'),
    # fputc to stdout  
    ("fputc('[', stdout)", 'printf("[")'),
    ("fputc(']', stdout)", 'printf("]")'),
    ("fputc('{', stdout)", 'printf("{")'),
    ("fputc('}', stdout)", 'printf("}")'),
    ("fputc('(', stdout)", 'printf("(")'),
    ("fputc(')', stdout)", 'printf(")")'),
    ("fputc('\\n', stdout)", 'printf("\\n")'),
    # fputc to stderr
    ("fputc('\\n', stderr)", 'fprintf(stderr, "\\n")'),
]

for old, new in replacements:
    if old in data:
        data = data.replace(old, new)

with open(sys.argv[1], 'w') as f:
    f.write(data)
