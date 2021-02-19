#ifndef OJIT_PARSER_H
#define OJIT_PARSER_H

#include "../asm_ir.h"
#include "../string_tools/ojit_string.h"
#include "lexer.h"

typedef struct s_Parser Parser;

Parser* create_parser(struct Lexer* lexer, struct HashTable* function_table, MemCtx* mem);
void parser_parse_source(Parser* parser);

#endif //OJIT_PARSER_H
