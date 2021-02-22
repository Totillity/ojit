#ifndef OJIT_PROGRAM_IR_H
#define OJIT_PROGRAM_IR_H

#include "string_tools/ojit_string.h"
#include "hash_table/hash_table.h"
#include "asm_ir.h"
#include "compiler.h"

typedef struct s_CompilerManager CompilerManager;

CompilerManager* create_CompilerManager(struct HashTable* functions);
bool CompilerManager_add_function(CompilerManager* program, struct FunctionIR* func);
bool CompilerManager_get_function(CompilerManager* program, String func_name, struct FunctionIR** func_loc);
bool CompilerManager_get_function_r(CompilerManager* program, char* name, size_t name_len, struct FunctionIR** func_loc);
struct CompiledFunction CompilerManager_compile_function(CompilerManager* program, char* name, size_t name_len);

String CompilerManager_read_file(CompilerManager* program, char* file_name);
void CompilerManager_parse_source(CompilerManager* program, String source);

#endif //OJIT_PROGRAM_IR_H
