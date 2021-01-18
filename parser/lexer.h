#ifndef OJIT_LEXER_H
#define OJIT_LEXER_H

#include "../string_tools/ojit_string.h"
#include "../asm_ir.h"

enum TokenType {
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_LET,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_AND,
    TOKEN_OR,

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


typedef struct Token_s {
    enum TokenType type;
    String text;
} Token;


struct Lexer;

struct Lexer* create_lexer(struct StringTable* table_ptr, String source);

Token lexer_peek_token(struct Lexer* lexer);
Token lexer_next_token(struct Lexer* lexer);
char* get_token_name(enum TokenType type);

#endif //OJIT_LEXER_H
