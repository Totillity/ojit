#include "program_ir.h"

#include "parser/parser.h"


bool create_program_ir(ProgramIR* program) {
    init_string_table(&program->strings, 256);
    init_hash_table(&program->function_table, 64);
    return true;
}

bool program_ir_add_function(ProgramIR* program, struct FunctionIR* func) {
    return hash_table_insert(&program->function_table, func->name, (uint64_t) func);
}

bool program_ir_get_function(ProgramIR* program, String func_name, struct FunctionIR** func_loc) {
    return hash_table_get(&program->function_table, func_name, (uint64_t*) func_loc);
}

bool program_ir_get_function_r(ProgramIR* program, char* name, size_t name_len, struct FunctionIR** func_loc) {
    String func_name = string_table_add(&program->strings, name, name_len);
    return hash_table_get(&program->function_table, func_name, (uint64_t*) func_loc);
}


String program_ir_read_file(ProgramIR* program_ir, char* file_name) {
    return read_file(&program_ir->strings, file_name);
}


void program_ir_parse_source(ProgramIR* program_ir, String source) {
    struct Lexer* lexer = create_lexer(&program_ir->strings, source);
    Parser* parser = create_parser(lexer, &program_ir->function_table);
    parser_parse_source(parser);
}