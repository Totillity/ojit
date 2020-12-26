cmake_minimum_required(VERSION 3.17)
project(ojit C)

set(CMAKE_C_STANDARD 11)

add_executable(ojit main.c parser.c parser.h asm_ir.c asm_ir.h asm_ir_builders.c asm_ir_builders.h string_tools/ojit_trie.c string_tools/ojit_trie.h string_tools/ojit_string.c string_tools/ojit_string.h)