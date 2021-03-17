cmake_minimum_required(VERSION 3.17)
project(ojit C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wno-attributes)

add_compile_definitions(OJIT_OPTIMIZATIONS)
add_compile_definitions(OJIT_READABLE_IR)

add_executable(ojit main.c parser.c parser.h asm_ir.h asm_ir_builders.c asm_ir_builders.h ojit_string.c ojit_string.h hash_table.c hash_table.h compiler.c compiler.h ojit_mem.c ojit_mem.h ojit_def.h jit_interpreter.c jit_interpreter.h ir_opt.c ir_opt.h ojit_def.c)