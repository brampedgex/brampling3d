import argparse
import re
import os

parser = argparse.ArgumentParser(prog='asset_build')
parser.add_argument('-o', '--output') # .asm output
parser.add_argument('--header') # .hpp output
parser.add_argument('--files', nargs='*') # input files to embed
args = parser.parse_args()

asm_file = open(args.output, "w")
header_file = open(args.header, "w")

header_file.write("""
// Generated with asset_build.py, do not modify!

#pragma once

""")

def get_symbol_name(file):
    return re.sub(r"[^a-zA-Z0-9_]", "_", file)

for file in args.files:
    symbol_name = get_symbol_name(file)
    asm_file.write(f"""
global _binary_{symbol_name}_start
global _binary_{symbol_name}_end
""")

asm_file.write(f"""
section .data
""")

for file in args.files:
    symbol_name = get_symbol_name(file)
    abs_path = os.path.abspath(file)
    asm_file.write(f"""
_binary_{symbol_name}_start:
incbin "{abs_path}"
_binary_{symbol_name}_end:
""")

for file in args.files:
    symbol_name = get_symbol_name(file)
    header_file.write(f"""
extern "C" const char _binary_{symbol_name}_start[];
extern "C" const char _binary_{symbol_name}_end[];
""")

asm_file.close()
header_file.close()
