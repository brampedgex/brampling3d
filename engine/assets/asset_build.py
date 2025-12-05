import argparse
import re
import os

parser = argparse.ArgumentParser(prog='asset_build')
parser.add_argument('-o', '--output') # .asm output
parser.add_argument('--header') # .hpp output
parser.add_argument('--nasm', action='store_true') # use nasm syntax
parser.add_argument('--apple', action='store_true') # build for apple
parser.add_argument('--files', nargs='*') # input files to embed
args = parser.parse_args()


def get_symbol_name(file):
    return re.sub(r"[^a-zA-Z0-9_]", "_", file)

# Write the header.
header_file = open(args.header, "w")
header_file.write("""
// Generated with asset_build.py, do not modify!

#pragma once

""")

for file in args.files:
    symbol_name = get_symbol_name(file)
    header_file.write(f"""
extern "C" const char _binary_{symbol_name}_start[];
extern "C" const char _binary_{symbol_name}_end[];
""")

header_file.close()


# Write the assembly file.
asm_file = open(args.output, "w")

if args.apple:
    symbol_prefix = "_"
    data_section = "__DATA,__data"
else:
    symbol_prefix = ""
    data_section = ".data"

if args.nasm:
    for file in args.files:
        symbol_name = get_symbol_name(file)
        asm_file.write(f"""
            global {symbol_prefix}_binary_{symbol_name}_start
            global {symbol_prefix}_binary_{symbol_name}_end
        """)

    asm_file.write(f"""
        section {data_section}
    """)

    for file in args.files:
        symbol_name = get_symbol_name(file)
        abs_path = os.path.abspath(file)
        asm_file.write(f"""
            {symbol_prefix}_binary_{symbol_name}_start:
            incbin "{abs_path}"
            {symbol_prefix}_binary_{symbol_name}_end:
        """)
else:
    for file in args.files:
        symbol_name = get_symbol_name(file)
        asm_file.write(f"""
            .global {symbol_prefix}_binary_{symbol_name}_start
            .global {symbol_prefix}_binary_{symbol_name}_end
        """)

    asm_file.write(f"""
        .section {data_section}
    """)

    for file in args.files:
        symbol_name = get_symbol_name(file)
        abs_path = os.path.abspath(file)
        asm_file.write(f"""
            {symbol_prefix}_binary_{symbol_name}_start:
            .incbin "{abs_path}"
            {symbol_prefix}_binary_{symbol_name}_end:
        """)

asm_file.close()
