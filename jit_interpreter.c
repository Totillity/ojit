#include "jit_interpreter.h"

#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include "compiler/compiler.h"


JIT* ojit_create_jit() {
    JIT* jit = malloc(sizeof(JIT));
    jit->string_mem = create_mem_ctx();
    jit->ir_mem = create_mem_ctx();
    init_string_table(&jit->strings, jit->string_mem);
    init_hash_table(&jit->function_records, jit->ir_mem);
    return jit;
}


bool jit_add_file(JIT* jit, char* file_name) {
    String source = read_file(&jit->strings, file_name);
    if (source) {
        MemCtx* parser_mem = create_mem_ctx();
        Parser* parser = create_parser(source, &jit->strings, &jit->function_records, jit->ir_mem, parser_mem);
        parser_parse_source(parser);
        destroy_mem_ctx(parser_mem);
        return true;
    } else {
        return false;
    }
}


JITFunc jit_get_function(JIT* jit, char* func_name, size_t name_len) {
    String func_name_str = string_table_add(&jit->strings, func_name, name_len);
    uint64_t func_ir_ptr;
    hash_table_get(&jit->function_records, STRING_KEY(func_name_str), &func_ir_ptr);
    return (void*) func_ir_ptr;
}

void* jit_compiled_callback(JIT* jit, String str) {
    struct FunctionIR* func_ir_ptr;
    hash_table_get(&jit->function_records, STRING_KEY(str), (uint64_t*) &func_ir_ptr);
    return jit_get_compiled_function(jit, func_ir_ptr, NULL);
}

void* jit_ir_callback(JIT* jit, String str) {
    struct FunctionIR* func_ir_ptr;
    hash_table_get(&jit->function_records, STRING_KEY(str), (uint64_t*) &func_ir_ptr);
    return func_ir_ptr;
}

void* jit_get_compiled_function(JIT* jit, JITFunc func, size_t* len) {
    if (func->compiled == NULL) {
        MemCtx* compiler_mem = create_mem_ctx();
        struct CompiledFunction compiled_func = ojit_compile_function(func, compiler_mem, (struct GetFunctionCallback) {
            .compiled_callback=jit_compiled_callback,
            .ir_callback=jit_ir_callback,
            .jit_ptr=jit
        });
        func->compiled = copy_to_executable(compiled_func.mem, compiled_func.size);
        if (len) {
            *len = compiled_func.size;
        }
        destroy_mem_ctx(compiler_mem);
    }
    return func->compiled;
}


void jit_dump_function(JIT* jit, JITFunc func, FILE* stream) {
    if (stream == NULL) {
        stream = stdout;
    }
    size_t code_len;
    uint8_t* code = jit_get_compiled_function(jit, func, &code_len);
    for (int i = 0; i < code_len; i++) {
        fprintf(stream, "%02x", code[i]);
    }
    fprintf(stream, "\n");
    fflush(stream);
}