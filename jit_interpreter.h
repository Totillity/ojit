#ifndef OJIT_JIT_INTERPRETER_H
#define OJIT_JIT_INTERPRETER_H

#include "ojit_mem.h"
#include "hash_table.h"
#include "ojit_string.h"
#include <stdio.h>

typedef struct s_JITState {
    MemCtx* ir_mem;
    MemCtx* string_mem;
    struct StringTable strings;
    struct HashTable function_records;
} JIT;

typedef struct FunctionIR* JITFunc;

#define jit_call_function(jit, func, typ, args...) ((typ) jit_get_compiled_function((jit), (func), NULL))(args)
JIT* ojit_create_jit();
bool jit_add_file(JIT* jit, char* file_name);
JITFunc jit_get_function(JIT* jit, char* func_name, size_t name_len);
void* jit_get_compiled_function(JIT* jit, JITFunc func, size_t* len);
void jit_dump_function(JIT* jit, JITFunc func, FILE* stream);

#endif //OJIT_JIT_INTERPRETER_H
