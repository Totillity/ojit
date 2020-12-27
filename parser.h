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
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR,
    TOKEN_STAR_STAR,
    TOKEN_SLASH,
    TOKEN_SLASH_SLASH,
    TOKEN_STAR_EQUAL,
    TOKEN_STAR_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_SLASH_SLASH_EQUAL,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
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
