#include "program_ir.h"
#include <stdlib.h>

#include "parser/parser.h"

struct s_CompilerManager {
    JState* jstate;

    struct StringTable strings;
    struct HashTable* function_table;
};

CompilerManager* create_CompilerManager(struct HashTable* functions) {
    CompilerManager* program = malloc(sizeof(CompilerManager));
    program->jstate = create_jstate();
    program->function_table = functions;
    init_string_table(&program->strings, program->jstate->string_mem);
    init_hash_table(program->function_table, program->jstate->ir_mem);
    return program;
}

bool CompilerManager_add_function(CompilerManager* program, struct FunctionIR* func) {
    return hash_table_insert(program->function_table, func->name, (uint64_t) func);
}

bool CompilerManager_get_function(CompilerManager* program, String func_name, struct FunctionIR** func_loc) {
    return hash_table_get(program->function_table, func_name, (uint64_t*) func_loc);
}

bool CompilerManager_get_function_r(CompilerManager* program, char* name, size_t name_len, struct FunctionIR** func_loc) {
    String func_name = string_table_add(&program->strings, name, name_len);
    return hash_table_get(program->function_table, func_name, (uint64_t*) func_loc);
}


struct CompiledFunction CompilerManager_compile_function(CompilerManager* program, char* name, size_t name_len) {
    struct FunctionIR* func;
    CompilerManager_get_function_r(program, name, name_len, &func);
    CState* cstate = create_cstate(program->jstate);
    return ojit_compile_function(cstate, func);
}


String CompilerManager_read_file(CompilerManager* program, char* file_name) {
    return read_file(&program->strings, file_name);
}

void CompilerManager_parse_source(CompilerManager* program, String source) {
    struct Lexer* lexer = create_lexer(&program->strings, source, program->jstate->ir_mem);
    Parser* parser = create_parser(lexer, program->function_table, program->jstate->ir_mem);
    parser_parse_source(parser);
}