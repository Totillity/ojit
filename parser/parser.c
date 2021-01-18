#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "lexer.h"
#include "../asm_ir_builders.h"
#include "../string_tools/ojit_trie.h"


typedef struct s_Parser {
    struct Lexer* lexer;
    struct IRBuilder* builder;
    struct HashTable* func_table;
} Parser;


Token parser_advance(Parser* parser) {
    return lexer_next_token(parser->lexer);
}

Token parser_peek(Parser* parser) {
    return lexer_peek_token(parser->lexer);
}

bool parser_peek_is(Parser* parser, enum TokenType type) {
    Token token = lexer_peek_token(parser->lexer);
    return token.type == type;
}

Token parser_expect(Parser* parser, enum TokenType type) {
    Token token = lexer_next_token(parser->lexer);
    if (token.type == type) {
        return token;
    } else {
        printf("Error: Expected %s, got %s", get_token_name(type), get_token_name(token.type));
        exit(-1);    // TODO exception handling
    }
}


typedef struct {
    bool also_lvalue;
    IRValue rvalue;
    String lvalue;
} ExpressionValue;

String get_lvalue(ExpressionValue value) {
    if (value.also_lvalue) {
        return value.lvalue;
    } else {
        printf("Attempted to access the lvalue of something which doesn't have one");
        exit(-1);    // TODO programmer error
    }
}
#ifdef SKIP_LVALUE_CHECK
#define LVALUE(v) ((v).lvalue)
#else
#define LVALUE(v) (get_lvalue(v))
#endif

#define RVALUE(v) ((v).rvalue)

#define WRAP_LVALUE(l, r) ((ExpressionValue) {.also_lvalue = true, .lvalue = (l), .rvalue = (r)})
#define WRAP_RVALUE(v) ((ExpressionValue) {.also_lvalue = false, .rvalue = (v)})


IRValue parse_expression(Parser* parser);


ExpressionValue parse_terminal(Parser* parser, bool lvalue) {
    Token curr = parser_peek(parser);
    switch (curr.type) {
        case TOKEN_IDENT: {
            parser_expect(parser, TOKEN_IDENT);
            IRValue value;
            hash_table_get(&parser->builder->current_block->variables, curr.text, (uint64_t*) &value); // TODO check
            if (lvalue) {
                return WRAP_LVALUE(curr.text, value);
            }
            return WRAP_RVALUE(value);
        }
        case TOKEN_NUMBER: {
            parser_expect(parser, TOKEN_NUMBER);
            int32_t num = 0;
            for (int i = 0; i < curr.text->length; i++) {
                num *= 10;
                num += curr.text->start_ptr[i] - 48;
            }
            IRValue value = builder_Int(parser->builder, num);
            return WRAP_RVALUE(value);
        }
        case TOKEN_LEFT_PAREN: {
            parser_expect(parser, TOKEN_LEFT_PAREN);
            IRValue expr = parse_expression(parser);
            parser_expect(parser, TOKEN_RIGHT_PAREN);
            return WRAP_RVALUE(expr);
        }
        default: {
            printf("Unexpected token: %s", get_token_name(curr.type));
            exit(-1); // TODO error handling
        }
    }
}


ExpressionValue parse_addition(Parser* parser, bool lvalue) {
    ExpressionValue expr = parse_terminal(parser, lvalue);

    Token curr = parser_peek(parser);
    while (curr.type == TOKEN_PLUS) {
        parser_expect(parser, TOKEN_PLUS);
        IRValue right = RVALUE(parse_terminal(parser, false));
        expr = WRAP_RVALUE(builder_Add(parser->builder, RVALUE(expr), right));
        curr = parser_peek(parser);
    }

    return expr;
}


ExpressionValue parse_assign(Parser* parser) {
    ExpressionValue expr = parse_addition(parser, true);
    if (parser_peek_is(parser, TOKEN_EQUAL)) {
        parser_expect(parser, TOKEN_EQUAL);
        String var = LVALUE(expr);
        IRValue right = RVALUE(parse_assign(parser));
        builder_set_variable(parser->builder, var, right);
        return WRAP_RVALUE(right);
    } else {
        return WRAP_RVALUE(RVALUE(expr));
    }
}


IRValue parse_expression(Parser* parser) {
    return RVALUE(parse_assign(parser));
}


void parse_let(Parser* parser) {
    parser_expect(parser, TOKEN_LET);
    Token var_name = parser_expect(parser, TOKEN_IDENT);
    parser_expect(parser, TOKEN_EQUAL);
    IRValue value = parse_expression(parser);
    builder_add_variable(parser->builder, var_name.text, value);
    parser_expect(parser, TOKEN_SEMICOLON);
}


void parse_return(Parser* parser) {
    parser_expect(parser, TOKEN_RETURN);
    IRValue ret_value = parse_expression(parser);
    builder_Return(parser->builder, ret_value);
    parser_expect(parser, TOKEN_SEMICOLON);
}


void parse_statement(Parser* parser) {
    Token curr = parser_peek(parser);
    switch (curr.type) {
        case TOKEN_RETURN: {
            parse_return(parser);
            break;
        }
        case TOKEN_LET: {
            parse_let(parser);
            break;
        }
        default: {
            parse_expression(parser);
            parser_expect(parser, TOKEN_SEMICOLON);
            break;
        }
    }
}


void parse_function(Parser* parser) {
    parser_expect(parser, TOKEN_DEF);

    Token name = parser_expect(parser, TOKEN_IDENT);
    struct FunctionIR* func = create_function(name.text);
    parser->builder = create_builder(func);

    parser_expect(parser, TOKEN_LEFT_PAREN);
    // TODO arguments
    parser_expect(parser, TOKEN_RIGHT_PAREN);

    parser_expect(parser, TOKEN_LEFT_BRACE);
    while (!parser_peek_is(parser, TOKEN_RIGHT_BRACE)) {
        parse_statement(parser);
    }
    parser_expect(parser, TOKEN_RIGHT_BRACE);

    parser->builder = NULL;
    hash_table_insert(parser->func_table, func->name, (uint64_t) func);
}


void parser_parse_source(Parser* parser) {
    struct Lexer* lexer = parser->lexer;

    Token curr_token = lexer_peek_token(lexer);
    while (curr_token.type != TOKEN_EOF) {
        switch (curr_token.type) {
            case TOKEN_DEF: {
                parse_function(parser);
                break;
            }
            default: {
                printf("Expected 'def', got %s with text %s", get_token_name(curr_token.type), null_terminate_string(curr_token.text));
                exit(-1); // TODO exception handling
            }
        }
        curr_token = lexer_peek_token(lexer);
    }
}


Parser* create_parser(struct Lexer* lexer, struct HashTable* function_table) {
    Parser* parser = malloc(sizeof(Parser));
    parser->lexer = lexer;
    parser->builder = NULL;
    parser->func_table = function_table;
    return parser;
}



