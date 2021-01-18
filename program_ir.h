#ifndef OJIT_PROGRAM_IR_H
#define OJIT_PROGRAM_IR_H

#include "string_tools/ojit_string.h"
#include "hash_table/hash_table.h"
#include "asm_ir.h"

struct s_ProgramIR {
    struct StringTable strings;
    struct HashTable function_table;
};
typedef struct s_ProgramIR ProgramIR;

bool create_program_ir(ProgramIR* program);
bool program_ir_add_function(ProgramIR* program, struct FunctionIR* func);
bool program_ir_get_function(ProgramIR* program, String func_name, struct FunctionIR** func_loc);
bool program_ir_get_function_r(ProgramIR* program, char* name, size_t name_len, struct FunctionIR** func_loc);

String program_ir_read_file(ProgramIR* program_ir, char* file_name);
void program_ir_parse_source(ProgramIR* program_ir, String source);

#endif //OJIT_PROGRAM_IR_H
