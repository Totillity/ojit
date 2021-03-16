#include "parser.h"
#include "asm_ir_builders.h"

#include <stdlib.h>
#include <string.h>

enum TokenType {
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_LET,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IF,
    TOKEN_ELSE,
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

struct Lexer {
    struct StringTable* table_ptr;
    String source;
    char* start;
    char* curr;

    String keywords[20];

    bool is_next_lexed;
    Token next_token;
};

struct TrieNode {
    size_t children_index[128];
    bool may_be_leaf;
    size_t index;
};

struct Trie {
    struct TrieNode* trie_node_array;  // first node is the root
    size_t num_nodes;
};

struct Trie construct_trie(char* sequences[], size_t num_sequences) {
    size_t array_size = 8;
    struct Trie trie = {.trie_node_array = calloc(array_size, sizeof(struct TrieNode)), .num_nodes = 1};
    trie.trie_node_array[0].index = 0;

    for (int i = 0; i < num_sequences; i++) {
        struct TrieNode* node = &trie.trie_node_array[0];
        size_t node_index = 0;
        char* sequence = sequences[i];
        size_t len = strlen(sequence);
        for (int j = 0; j < len; j++) {
            char chr = sequence[j];
            if (node->children_index[chr] == 0) {
                // add a node for/at this character
                if (array_size <= trie.num_nodes) {
                    struct TrieNode* new_arr = calloc(array_size*2, sizeof(struct TrieNode));
                    memcpy(new_arr, trie.trie_node_array, sizeof(struct TrieNode) * array_size);
                    free(trie.trie_node_array);
                    trie.trie_node_array = new_arr;
                    array_size *= 2;
                    node = &trie.trie_node_array[node_index];  // since everything's been moved, make the node pointer show the new location
                }
                node->children_index[chr] = trie.num_nodes;
                struct TrieNode* new_node = &trie.trie_node_array[node->children_index[chr]];
                new_node->index = trie.num_nodes;
                trie.num_nodes++;
            }
            size_t next_index = node->children_index[chr];
            node = &trie.trie_node_array[next_index];
            node_index = node->index;
        }
        node->may_be_leaf = true;
    }

    trie.trie_node_array = realloc(trie.trie_node_array, sizeof(struct TrieNode) * trie.num_nodes);
    return trie;
}

struct Trie basic_token_trie;
struct TrieNode* basic_token_trie_root = NULL;
char* BASIC_TOKENS[28] = {
        "{", "}", "(", ")",
        "=", "==",
        "!", "!=", "<", ">", "<=", ">=",
        "+", "-", "+=", "-=",
        "*", "**", "/", "//", "*=", "**=", "/=", "//=",
        ".", ":", ",", ";",
};

enum TokenType BASIC_TOKEN_INDICIES[28] = {
        [0] =  TOKEN_LEFT_BRACE,
        [1] =  TOKEN_RIGHT_BRACE,
        [2] =  TOKEN_LEFT_PAREN,
        [3] =  TOKEN_RIGHT_PAREN,
        [4] =  TOKEN_EQUAL,
        [5] =  TOKEN_EQUAL_EQUAL,
        [6] =  TOKEN_BANG,
        [7] =  TOKEN_BANG_EQUAL,
        [8] =  TOKEN_LESS,
        [9] =  TOKEN_GREATER,
        [10] = TOKEN_LESS_EQUAL,
        [11] = TOKEN_GREATER_EQUAL,
        [12] = TOKEN_PLUS,
        [13] = TOKEN_MINUS,
        [14] = TOKEN_PLUS_EQUAL,
        [15] = TOKEN_MINUS_EQUAL,
        [16] = TOKEN_STAR,
        [17] = TOKEN_STAR_STAR,
        [18] = TOKEN_SLASH,
        [19] = TOKEN_SLASH_SLASH,
        [20] = TOKEN_STAR_EQUAL,
        [21] = TOKEN_STAR_STAR_EQUAL,
        [22] = TOKEN_SLASH_EQUAL,
        [23] = TOKEN_SLASH_SLASH_EQUAL,
        [24] = TOKEN_DOT,
        [25] = TOKEN_COLON,
        [26] = TOKEN_COMMA,
        [27] = TOKEN_SEMICOLON,
};


char* type_names[40] = {
        [TOKEN_DEF] = "'def'",
        [TOKEN_RETURN] = "'return'",
        [TOKEN_LET] = "'let'",
        [TOKEN_WHILE] = "'while'",
        [TOKEN_FOR] = "'for'",
        [TOKEN_IF] = "'if'",
        [TOKEN_ELSE] = "'else'",
        [TOKEN_AND] = "'and'",
        [TOKEN_OR] = "'or'",
        [TOKEN_IDENT] = "an identifier",
        [TOKEN_NUMBER] = "a number",
        [TOKEN_LEFT_PAREN] = "'('",
        [TOKEN_RIGHT_PAREN] = "')'",
        [TOKEN_LEFT_BRACE] = "'{'",
        [TOKEN_RIGHT_BRACE] = "'}'",
        [TOKEN_EQUAL] = "'='",
        [TOKEN_EQUAL_EQUAL] = "'=='",
        [TOKEN_BANG] = "'!'",
        [TOKEN_BANG_EQUAL] = "'!='",
        [TOKEN_LESS] = "'<'",
        [TOKEN_GREATER] = "'>'",
        [TOKEN_LESS_EQUAL] = "'<='",
        [TOKEN_GREATER_EQUAL] = "'>='",
        [TOKEN_PLUS] = "'+'",
        [TOKEN_MINUS] = "'-'",
        [TOKEN_PLUS_EQUAL] = "'+='",
        [TOKEN_MINUS_EQUAL] = "'-='",
        [TOKEN_STAR] = "'*'",
        [TOKEN_STAR_STAR] = "'**'",
        [TOKEN_SLASH] = "'/'",
        [TOKEN_SLASH_SLASH] = "'//'",
        [TOKEN_STAR_EQUAL] = "'*='",
        [TOKEN_STAR_STAR_EQUAL] = "'**='",
        [TOKEN_SLASH_EQUAL] = "'/='",
        [TOKEN_SLASH_SLASH_EQUAL] = "'//='",
        [TOKEN_DOT] = "'.'",
        [TOKEN_COLON] = "':'",
        [TOKEN_COMMA] = "','",
        [TOKEN_SEMICOLON] = "';'",
        [TOKEN_EOF] = "the end of the file\0",
};


void init_trie() {
    basic_token_trie = construct_trie(BASIC_TOKENS, 28);
    basic_token_trie_root = &basic_token_trie.trie_node_array[0];
}

#define IS_ALPHA(chr) (('a' <= (chr) && (chr) <= 'z') || ('A' <= (chr) && (chr) <= 'Z') || ((chr) == '_'))
#define IS_NUM(chr) ('0' <= (chr) && (chr) <= '9')
#define IS_ALPHANUM(chr) (IS_ALPHA(chr) || IS_NUM(chr))
#define IS_WHITESPACE(chr) ((chr) == ' ' || (chr) == '\t' || (chr) == '\r' || (chr) == '\n')


Token lexer_emit_token(struct Lexer* lexer, enum TokenType type) {
    lexer->next_token = (Token) {
            .type = type,
            .text = string_table_add(lexer->table_ptr, lexer->start, lexer->curr - lexer->start),
    };
    lexer->is_next_lexed = true;
    return lexer->next_token;
}


Token lexer_emit_ident(struct Lexer* lexer) {
    String text = string_table_add(lexer->table_ptr, lexer->start, lexer->curr - lexer->start);
    enum TokenType type;
    if (string_equal(text, lexer->keywords[TOKEN_DEF])) {
        type = TOKEN_DEF;
    } else if (string_equal(text, lexer->keywords[TOKEN_RETURN])) {
        type = TOKEN_RETURN;
    } else if (string_equal(text, lexer->keywords[TOKEN_LET])) {
        type = TOKEN_LET;
    } else if (string_equal(text, lexer->keywords[TOKEN_IF])) {
        type = TOKEN_IF;
    } else if (string_equal(text, lexer->keywords[TOKEN_ELSE])) {
        type = TOKEN_ELSE;
    } else {
        type = TOKEN_IDENT;
    }
    lexer->next_token = (Token) { .type = type, .text = text };
    lexer->is_next_lexed = true;
    return lexer->next_token;
}


char lexer_advance(struct Lexer* lexer) {
    lexer->curr++;
    return lexer->curr[0];
}

char lexer_peek(struct Lexer* lexer) {
    return lexer->curr[0];
}

bool lexer_at_end(struct Lexer* lexer) {
    return lexer->curr - lexer->source->start_ptr >= lexer->source->length;
}

bool lexer_trie_match(struct Lexer* lexer, struct TrieNode* trie_root, enum TokenType* index_ref) {
    char* old_curr = lexer->curr;

    char curr = lexer_peek(lexer);

    struct TrieNode* node = trie_root;
    while (node->children_index[curr] != 0) {
        node = &trie_root[node->children_index[curr]];
        curr = lexer_advance(lexer);
    }
    if (node->may_be_leaf) {
        lexer_emit_token(lexer, index_ref[node->index-1]);
        return true;
    } else {
        lexer->curr = old_curr;
        return false;
    }
}


Token lexer_peek_token(struct Lexer* lexer) {
    if (lexer->is_next_lexed) {
        return lexer->next_token;
    } else {
        if (lexer_at_end(lexer)) {  // TODO find a more efficient way to do this
            lexer->start = lexer->curr;
            return lexer_emit_token(lexer, TOKEN_EOF);
        }
        char curr = lexer_peek(lexer);
        while (!lexer_at_end(lexer) && IS_WHITESPACE(curr)) {
            curr = lexer_advance(lexer);
        }
        lexer->start = lexer->curr;
        if (lexer_trie_match(lexer, basic_token_trie_root, BASIC_TOKEN_INDICIES)) {
            return lexer->next_token;
        } else if (IS_ALPHA(curr)) {
            while (IS_ALPHANUM(curr)) {
                curr = lexer_advance(lexer);
            }
            return lexer_emit_ident(lexer);
        } else if (IS_NUM(curr)) {
            while (IS_NUM(curr)) {
                curr = lexer_advance(lexer);
            }
            return lexer_emit_token(lexer, TOKEN_NUMBER);
        } else if (lexer_at_end(lexer)) {  // TODO find a more efficient way to do this and fold these two copies
            return lexer_emit_token(lexer, TOKEN_EOF);
        } else {
            ojit_error();
            ojit_build_error_chars("Error: Unrecognized character ");
            ojit_build_error_char(curr);
            ojit_error();
            exit(0);
        }
    }
}

Token lexer_next_token(struct Lexer* lexer) {
    Token token = lexer_peek_token(lexer);
    lexer->is_next_lexed = false;
    return token;
}


char* get_token_name(enum TokenType type) {
    return type_names[type];
}

struct Lexer* create_lexer(struct StringTable* table_ptr, String source, MemCtx* parser_mem) {
    struct Lexer* lexer = ojit_alloc(parser_mem, sizeof(struct Lexer));
    lexer->table_ptr = table_ptr;

    if (basic_token_trie_root == NULL) {
        init_trie();
    }

    lexer->keywords[TOKEN_DEF] = string_table_add(lexer->table_ptr, "def", 3);
    lexer->keywords[TOKEN_RETURN] = string_table_add(lexer->table_ptr, "return", 6);
    lexer->keywords[TOKEN_LET] = string_table_add(lexer->table_ptr, "let", 3);
    lexer->keywords[TOKEN_IF] = string_table_add(lexer->table_ptr, "if", 2);
    lexer->keywords[TOKEN_ELSE] = string_table_add(lexer->table_ptr, "else", 4);

    lexer->source = source;
    lexer->start = source->start_ptr;
    lexer->curr = source->start_ptr;
    lexer->is_next_lexed = false;

    return lexer;
}


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
        switch (value.rvalue->base.id) {
            case ID_GLOBAL_IR:
                ojit_build_error_chars("Cannot assign to global values");
                break;
            default:
                ojit_build_error_chars("Attempted to access the lvalue of something which doesn't have one");
                break;
        }
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


// region Parse Expression
IRValue parse_expression(Parser* parser);

ExpressionValue parse_terminal(Parser* parser, bool lvalue) {
    Token curr = parser_peek(parser);
    switch (curr.type) {
        case TOKEN_IDENT: {
            parser_expect(parser, TOKEN_IDENT);
            IRValue value;
            if (hash_table_get(&parser->builder->current_block->variables, STRING_KEY(curr.text), (uint64_t*) &value)) {
                return lvalue ? WRAP_LVALUE(curr.text, value) : WRAP_RVALUE(value);
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

ExpressionValue parse_compare(Parser* parser, bool lvalue) {
    ExpressionValue expr = parse_addition(parser, lvalue);

    IRValue right;
    while (true) {
        Token curr = parser_peek(parser);
        enum Comparison cmp;
        switch (curr.type) {
            case TOKEN_LESS: cmp = IF_LESS; break;
            case TOKEN_GREATER: cmp = IF_GREATER; break;
            default: goto at_end;
        }
        parser_advance(parser);
        right = RVALUE(parse_addition(parser, false));
        expr = WRAP_RVALUE(builder_Cmp(parser->builder, cmp, RVALUE(expr), right));
    }
    at_end:

    return expr;
}


ExpressionValue parse_assign(Parser* parser) {
    ExpressionValue expr = parse_compare(parser, true);
//    ExpressionValue expr = parse_addition(parser, true);
    if (parser_peek_is(parser, TOKEN_EQUAL)) {
        parser_expect(parser, TOKEN_EQUAL);
        String var = LVALUE(expr);  // TODO add check here so it doesn't just fail
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
// endregion

// region Parse Statement
void parse_statement(Parser* parser);

void parse_if(Parser* parser) {
    parser_expect(parser, TOKEN_IF);
    parser_expect(parser, TOKEN_LEFT_PAREN);
    IRValue cond = parse_expression(parser);
    parser_expect(parser, TOKEN_RIGHT_PAREN);

    struct BlockIR* then_block = builder_add_block(parser->builder);
    struct BlockIR* else_block = builder_add_block(parser->builder);
    struct BlockIR* after_block = builder_add_block(parser->builder);
    builder_CBranch(parser->builder, cond, then_block, else_block);

    builder_enter_block(parser->builder, then_block);
    parse_statement(parser);
    if (parser->builder->current_block->terminator.ir_base.id == ID_TERM_NONE) {
        builder_Branch(parser->builder, after_block);
    }

    parser_expect(parser, TOKEN_ELSE);
    builder_enter_block(parser->builder, else_block);
    parse_statement(parser);
    if (parser->builder->current_block->terminator.ir_base.id == ID_TERM_NONE) {
        builder_Branch(parser->builder, after_block);
    }

    builder_enter_block(parser->builder, after_block);
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
        case TOKEN_RETURN: parse_return(parser); break;
        case TOKEN_LET: parse_let(parser); break;
        case TOKEN_IF: parse_if(parser); break;
        default: parse_expression(parser); parser_expect(parser, TOKEN_SEMICOLON); break;
    }
}
// endregion

void parse_function(Parser* parser) {
    parser_expect(parser, TOKEN_DEF);

    Token name = parser_expect(parser, TOKEN_IDENT);
    struct FunctionIR* func = create_function(name.text, parser->ir_mem);
    IRBuilder* builder = parser->builder = create_builder(func, parser->ir_mem);

    parser_expect(parser, TOKEN_LEFT_PAREN);
    while (!parser_peek_is(parser, TOKEN_RIGHT_PAREN)) {
        Token param_name = parser_expect(parser, TOKEN_IDENT);
        IRValue param = builder_add_parameter(builder, param_name.text);
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
    hash_table_insert(parser->func_table, STRING_KEY(func->name), (uint64_t) func);
}


void parser_parse_source(Parser* parser) {
    struct Lexer* lexer = parser->lexer;

    Token curr_token = lexer_peek_token(lexer);
    while (curr_token.type != TOKEN_EOF) {
        switch (curr_token.type) {
            case TOKEN_DEF: parse_function(parser); break;
            default:
                ojit_new_error();
                ojit_build_error_chars("Expected 'def', got ");
                ojit_build_error_chars(get_token_name(curr_token.type));
                ojit_build_error_chars(" with text ");
                ojit_build_error_String(curr_token.text);
                ojit_error();
                exit(0);
        }
        curr_token = lexer_peek_token(lexer);
    }
}

Parser* create_parser(String source, struct StringTable* string_table, struct HashTable* function_table, MemCtx* ir_mem, MemCtx* parser_mem) {
    struct Lexer* lexer = create_lexer(string_table, source, parser_mem);
    Parser* parser = ojit_alloc(parser_mem, sizeof(Parser));
    parser->lexer = lexer;
    parser->builder = NULL;
    parser->func_table = function_table;
    parser->ir_mem = ir_mem;
    return parser;
}



