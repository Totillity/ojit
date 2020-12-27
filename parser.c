#include "parser.h"

#include <stdio.h>
#include <string.h>


struct Trie basic_token_trie;
struct TrieNode* basic_token_trie_root;
char* BASIC_TOKENS[28] = {
        "{", "}", "(", ")",
        "=", "==",
        "!", "!=", "<", ">", "<=", ">=",
        "+", "-", "+=", "-=",
        "*", "**", "/", "//", "*=", "**=", "/=", "//=",
        ".", ":", ",", ";",
};

struct Trie keywords_trie;
struct TrieNode* keywords_trie_root;
char* KEYWORDS[1] = {
    "def",
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


enum TokenType KEYWORDS_INDICES[1] = {
        [0] = TOKEN_DEF,
};


char* type_names[32] = {
        [TOKEN_DEF] = "'def'\0",
        [TOKEN_IDENT] = "an identifier\0",
        [TOKEN_NUMBER] = "a number\0",
        [TOKEN_LEFT_PAREN] = "'('\0",
        [TOKEN_RIGHT_PAREN] = "')'\0",
        [TOKEN_LEFT_BRACE] = "'{'\0",
        [TOKEN_RIGHT_BRACE] = "'}'\0",
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


void init_parser() {
    basic_token_trie = construct_trie(BASIC_TOKENS, 28);
    basic_token_trie_root = &basic_token_trie.trie_node_array[0];

    keywords_trie = construct_trie(KEYWORDS, 1);
    keywords_trie_root = &keywords_trie.trie_node_array[0];
}


void print_token(Token token) {
    char* text_buf = malloc(token.text.len+1);
    memcpy(text_buf, token.text.start_ptr, token.text.len);
    text_buf[token.text.len] = '\0';
    printf("TOKEN(type: %s, text: '%s')\n", type_names[token.type], text_buf);
}


struct Source* read_file(char* path) {
    FILE* file = fopen(path, "r"); // TODO check for null
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    struct Source* source = (struct Source*) malloc(sizeof(struct Source) + file_size + 1);
    source->size = fread(&source->text, sizeof(char), file_size, file);
    source->text[source->size] = '\0';
    fclose(file);
    return source;
}

struct LexState* create_lexer(struct Source* source) {
    struct LexState* state = malloc(sizeof(struct LexState));
    state->source = source;
    state->start = source->text;
    state->curr = source->text;
    state->is_next_lexed = false;
    return state;
}

#define IS_ALPHA(chr) (('a' <= (chr) && (chr) <= 'z') || ('A' <= (chr) && (chr) <= 'Z') || ((chr) == '_'))
#define IS_NUM(chr) ('0' <= (chr) && (chr) <= '9')
#define IS_ALPHANUM(chr) (IS_ALPHA(chr) || IS_NUM(chr))
#define IS_WHITESPACE(chr) ((chr) == ' ' || (chr) == '\t' || (chr) == '\r' || (chr) == '\n')


Token lexer_emit_token(struct LexState* lexer, enum TokenType type) {
    lexer->next_token = (struct Token) {
        .type = type,
        .text = (struct String) {.start_ptr = lexer->start, .len = lexer->curr - lexer->start}
    };
    lexer->is_next_lexed = true;
    return lexer->next_token;
}


char lexer_advance(struct LexState* lexer) {
    lexer->curr++;
    return lexer->curr[0];
}


char lexer_peek(struct LexState* lexer) {
    return lexer->curr[0];
}


bool lexer_at_end(struct LexState* lexer) {
    return lexer->curr - lexer->source->text >= lexer->source->size;
}


bool lexer_trie_match(struct LexState* lexer, struct TrieNode* trie_root, enum TokenType* index_ref) {
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


Token lexer_peek_token(struct LexState* lexer) {
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
            if (lexer_trie_match(lexer, keywords_trie_root, KEYWORDS_INDICES)) {
                return lexer->next_token;
            }
            while (IS_ALPHANUM(curr)) {
                curr = lexer_advance(lexer);
            }
            return lexer_emit_token(lexer, TOKEN_IDENT);
        } else if (IS_NUM(curr)) {
            while (IS_NUM(curr)) {
                curr = lexer_advance(lexer);
            }
            return lexer_emit_token(lexer, TOKEN_NUMBER);
        } else if (lexer_at_end(lexer)) {  // TODO find a more efficient way to do this and fold these two copies
            return lexer_emit_token(lexer, TOKEN_EOF);
        } else {
            printf("Error: Unrecognized character '%c' (0x%02x).\n", curr, curr);
            exit(-1);
        }
    }
}


Token lexer_next_token(struct LexState* lexer) {
    Token token = lexer_peek_token(lexer);
    lexer->is_next_lexed = false;
    return token;
}


struct ParseState* create_parser(struct LexState* lexer) {
    struct ParseState* parser = malloc(sizeof(struct ParseState));
    parser->lexer = lexer;
    return parser;
}


Token parser_expect(struct ParseState* parser, enum TokenType type) {
    Token token = lexer_next_token(parser->lexer);
    if (token.type == type) {
        return token;
    } else {
        printf("Error: Expected %s, got %s", type_names[type], type_names[token.type]);
        exit(-1);
    }
}


bool parser_peek(struct ParseState* parser, enum TokenType type) {
    Token token = lexer_peek_token(parser->lexer);
    return token.type == type;
}


struct FunctionIR* parse_function(struct ParseState* parser) {
    Token start = parser_expect(parser, TOKEN_DEF);
    Token name = parser_expect(parser, TOKEN_IDENT);
    parser_expect(parser, TOKEN_LEFT_PAREN);
    parser_expect(parser, TOKEN_RIGHT_PAREN);

    parser_expect(parser, TOKEN_LEFT_BRACE);
    while (!parser_peek(parser, TOKEN_RIGHT_BRACE)) {

    }
    parser_expect(parser, TOKEN_RIGHT_BRACE);
}
