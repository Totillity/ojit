#include "jit_interpreter.h"

#include <stdlib.h>
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
    struct Lexer* lexer = create_lexer(&jit->strings, source, jit->jstate->ir_mem);
    Parser* parser = create_parser(lexer, &jit->function_records, jit->jstate->ir_mem);
    parser_parse_source(parser);
}


JITFunc jit_get_function(JIT* jit, char* func_name, size_t name_len) {
    String func_name_str = string_table_add(&jit->strings, func_name, name_len);
    uint64_t func_ir_ptr;
    hash_table_get(&jit->function_records, func_name_str, &func_ir_ptr);
    return (void*) func_ir_ptr;
}

void* jit_compiled_function(JIT* jit, JITFunc func) {
    CState* cstate = create_cstate(jit->jstate);
    struct CompiledFunction compiled_func = ojit_compile_function(cstate, func);
    void* exec_func = copy_to_executable(compiled_func.mem, compiled_func.size);
    destroy_cstate(cstate);
    return exec_func;
}