#ifndef OJIT_PARSER_H
#define OJIT_PARSER_H

#include <stdlib.h>
#include <stdbool.h>

#include "asm_ir.h"
#include "string_tools/ojit_string.h"
#include "string_tools/ojit_trie.h"

struct Source {
    size_t size;
    char text[];
};


enum TokenType {
    TOKEN_DEF,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_EOF
};


typedef struct Token {
    enum TokenType type;
    struct String text;
} Token;


struct LexState {
    struct Source* source;
    char* start;
    char* curr;

    bool is_next_lexed;
    Token next_token;
};


struct ParseState {
    struct LexState* lexer;
};


struct Source* read_file(char* path);

void init_parser();
void print_token(Token token);

struct LexState* create_lexer(struct Source* source);
Token lexer_next_token(struct LexState* lexer);

#endif //OJIT_PARSER_H
