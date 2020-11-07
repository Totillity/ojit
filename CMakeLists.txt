cmake_minimum_required(VERSION 3.17)
project(ojit C)

set(CMAKE_C_STANDARD 11)

add_executable(ojit main.c asm-gen.c asm-gen.h asm_ir.c asm_ir.h)