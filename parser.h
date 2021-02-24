#ifndef OJIT_PARSER_H
#define OJIT_PARSER_H

#include "asm_ir.h"
#include "ojit_string.h"

typedef struct s_Parser Parser;

Parser* create_parser(String source, struct StringTable* string_table, struct HashTable* function_table, MemCtx* ir_mem, MemCtx* parser_mem);
void parser_parse_source(Parser* parser);

#endif //OJIT_PARSER_H
