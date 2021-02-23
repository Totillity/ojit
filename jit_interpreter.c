#include "jit_interpreter.h"

#include <stdlib.h>
#include <stdio.h>
#include "parser/lexer.h"
#include "parser/parser.h"
#include "compiler.h"


JIT* ojit_create_jit() {
    JIT* jit = malloc(sizeof(JIT));
    jit->jstate = create_jstate();
    init_string_table(&jit->strings, jit->jstate->string_mem);
    init_hash_table(&jit->function_records, jit->jstate->ir_mem);
    return jit;
}


void jit_add_file(JIT* jit, char* file_name) {
    String source = read_file(&jit->strings, file_name);
    MemCtx* parser_mem = create_mem_ctx();
    struct Lexer* lexer = create_lexer(&jit->strings, source, parser_mem);
    Parser* parser = create_parser(lexer, &jit->function_records, jit->jstate->ir_mem, parser_mem);
    parser_parse_source(parser);
    destroy_mem_ctx(parser_mem);
}


JITFunc jit_get_function(JIT* jit, char* func_name, size_t name_len) {
    String func_name_str = string_table_add(&jit->strings, func_name, name_len);
    uint64_t func_ir_ptr;
    hash_table_get(&jit->function_records, func_name_str, &func_ir_ptr);
    return (void*) func_ir_ptr;
}

void* jit_func_callback(JIT* jit, String str) {
    ojit_new_error();
    ojit_build_error_chars("Fetch: ");
    ojit_build_error_String(str);
    ojit_error();
    struct FunctionIR* func_ir_ptr;
    hash_table_get(&jit->function_records, str, (uint64_t*) &func_ir_ptr);
    return jit_get_compiled_function(jit, func_ir_ptr, NULL);
}

void* jit_get_compiled_function(JIT* jit, JITFunc func, size_t* len) {
    if (func->compiled == NULL) {
        CState* cstate = create_cstate(jit->jstate);
        struct CompiledFunction compiled_func = ojit_compile_function(cstate, func,
                (struct GetFunctionCallback) {.callback=jit_func_callback, .arg=jit});
        func->compiled = copy_to_executable(compiled_func.mem, compiled_func.size);
        if (len) {
            *len = compiled_func.size;
        }
        destroy_cstate(cstate);
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