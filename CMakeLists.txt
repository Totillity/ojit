cmake_minimum_required(VERSION 3.17)
project(ojit C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wno-attributes)

add_executable(ojit main.c parser/parser.c parser/parser.h asm_ir.c asm_ir.h asm_ir_builders.c asm_ir_builders.h string_tools/ojit_trie.c string_tools/ojit_trie.h string_tools/ojit_string.c string_tools/ojit_string.h hash_table/hash_table_impl.c source_compiler.c source_compiler.h hash_table/hash_table.h parser/lexer.c parser/lexer.h compiler.c compiler.h program_ir.c program_ir.h ojit_mem.c ojit_mem.h ojit_err.c ojit_err.h ojit_def.h ojit_state.c ojit_state.h)