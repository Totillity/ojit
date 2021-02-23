#include "lexer.h"
#include "../string_tools/ojit_trie.h"
#include "../ojit_err.h"

struct Lexer {
        struct StringTable* table_ptr;
        String source;
        char* start;
        char* curr;

        String keywords[20];

        bool is_next_lexed;
        Token next_token;
};

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


char* type_names[38] = {
        [TOKEN_DEF] = "'def'",
        [TOKEN_RETURN] = "'return'",
        [TOKEN_LET] = "'let'",
        [TOKEN_WHILE] = "'while'",
        [TOKEN_FOR] = "'for'",
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


//void print_token(Token token) {
//    char* text_buf = malloc(token.text->length+1);
//    memcpy(text_buf, token.text->start_ptr, token.text->length);
//    text_buf[token.text->length] = '\0';
//    printf("TOKEN(type: %s, text: '%s')\n", type_names[token.type], text_buf);
//}

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

    lexer->source = source;
    lexer->start = source->start_ptr;
    lexer->curr = source->start_ptr;
    lexer->is_next_lexed = false;

    return lexer;
}