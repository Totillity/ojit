#include "parser.h"
#include "../asm_ir_builders.h"
#include "../string_tools/ojit_trie.h"
#include "../ojit_err.h"


typedef struct s_Parser {
    struct Lexer* lexer;
    IRBuilder* builder;
    struct HashTable* func_table;
    MemCtx* ir_mem;
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
        ojit_new_error();
        ojit_build_error_chars("Error: Expected ");
        ojit_build_error_chars(get_token_name(type));
        ojit_build_error_chars(", got ");
        ojit_build_error_chars(get_token_name(token.type));
        ojit_error();
        exit(0);
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
        ojit_new_error();
        ojit_build_error_chars("Attempted to access the lvalue of something which doesn't have one");
        ojit_error();
        exit(0);
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
            bool success = hash_table_get(&parser->builder->current_block->variables, curr.text, (uint64_t*) &value);
            if (success) {
                if (lvalue) {
                    return WRAP_LVALUE(curr.text, value);
                }
                return WRAP_RVALUE(value);
            } else {
                return WRAP_RVALUE(builder_Global(parser->builder, curr.text));
            }
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
            ojit_new_error();
            ojit_build_error_chars("Unexpected token: ");
            ojit_build_error_chars(get_token_name(curr.type));
            ojit_error();
            exit(0);
        }
    }
}


ExpressionValue parse_call(Parser* parser, bool lvalue) {
    ExpressionValue expr = parse_terminal(parser, lvalue);

    while (true) {
        Token curr = parser_peek(parser);
        switch (curr.type) {
            case TOKEN_LEFT_PAREN:
                expr = WRAP_RVALUE(builder_Call(parser->builder, RVALUE(expr)));
                parser_expect(parser, TOKEN_LEFT_PAREN);
                while (!parser_peek_is(parser, TOKEN_RIGHT_PAREN)) {
                    IRValue arg = parse_expression(parser);
                    builder_Call_argument(RVALUE(expr), arg);
                    if (parser_peek_is(parser, TOKEN_COMMA)) {
                        parser_expect(parser, TOKEN_COMMA);
                        continue;
                    } else {
                        break;
                    }
                }
                parser_expect(parser, TOKEN_RIGHT_PAREN);
                break;
            default:
                goto at_end;
        }
    }
    at_end:
    return expr;
}


ExpressionValue parse_addition(Parser* parser, bool lvalue) {
    ExpressionValue expr = parse_call(parser, lvalue);

    IRValue right;
    while (true) {
        Token curr = parser_peek(parser);
        switch (curr.type) {
            case TOKEN_PLUS:
                parser_expect(parser, TOKEN_PLUS);
                right = RVALUE(parse_call(parser, false));
                expr = WRAP_RVALUE(builder_Add(parser->builder, RVALUE(expr), right));
                break;
            case TOKEN_MINUS:
                parser_expect(parser, TOKEN_MINUS);
                right = RVALUE(parse_call(parser, false));
                expr = WRAP_RVALUE(builder_Sub(parser->builder, RVALUE(expr), right));
                break;
            default:
                goto at_end;
        }
    }
    at_end:

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
    struct FunctionIR* func = create_function(name.text, parser->ir_mem);
    IRBuilder* builder = parser->builder = create_builder(func, parser->ir_mem);

    parser_expect(parser, TOKEN_LEFT_PAREN);
    while (!parser_peek_is(parser, TOKEN_RIGHT_PAREN)) {
        Token param_name = parser_expect(parser, TOKEN_IDENT);
        IRValue param = builder_add_parameter(builder);
        builder_add_variable(builder, param_name.text, param);
        if (parser_peek_is(parser, TOKEN_COMMA)) {
            parser_expect(parser, TOKEN_COMMA);
            continue;
        } else {
            break;
        }
    }
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
                ojit_new_error();
                ojit_build_error_chars("Expected 'def', got ");
                ojit_build_error_chars(get_token_name(curr_token.type));
                ojit_build_error_chars(" with text ");
                ojit_build_error_String(curr_token.text);
                ojit_error();
                exit(0);
            }
        }
        curr_token = lexer_peek_token(lexer);
    }
}

Parser* create_parser(struct Lexer* lexer, struct HashTable* function_table, MemCtx* ir_mem, MemCtx* parser_mem) {
    Parser* parser = ojit_alloc(parser_mem, sizeof(Parser));
    parser->lexer = lexer;
    parser->builder = NULL;
    parser->func_table = function_table;
    parser->ir_mem = ir_mem;
    return parser;
}



