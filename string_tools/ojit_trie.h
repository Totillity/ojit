#ifndef OJIT_OJIT_TRIE_H
#define OJIT_OJIT_TRIE_H

#include <stdbool.h>
#include <stdint.h>

//#define TRIE_CHAR_START 0x20
//#define TRIE_CHAR_END 0x7F
//#define TRIE_CHAR_SIZE (TRIE_CHAR_END - TRIE_CHAR_START + 1)

struct TrieNode {
    size_t children_index[128];
    bool may_be_leaf;
    size_t index;
};


struct Trie {
    struct TrieNode* trie_node_array;  // first node is the root
    size_t num_nodes;
};


struct Trie construct_trie(char* sequences[], size_t num_sequences);

#endif //OJIT_OJIT_TRIE_H
