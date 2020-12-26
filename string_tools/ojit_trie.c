#include "ojit_trie.h"


#include <stdlib.h>
#include <string.h>


struct Trie construct_trie(char* sequences[], size_t num_sequences) {
    size_t array_size = 8;
    struct Trie trie = {.trie_node_array = calloc(array_size, sizeof(struct TrieNode)), .num_nodes = 1};

    for (int i = 0; i < num_sequences; i++) {
        struct TrieNode* node = &trie.trie_node_array[0];
        char* sequence = sequences[i];
        size_t len = strlen(sequence);
        for (int j = 0; j < len; j++) {
            char chr = sequence[j];
            if (node->children[chr] == NULL) {
                // add a node at this character
                if (array_size <= trie.num_nodes) {
                    size_t offset = node - trie.trie_node_array;
                    struct TrieNode* new_arr = calloc(array_size*2, sizeof(struct TrieNode));
                    memcpy(new_arr, trie.trie_node_array, sizeof(struct TrieNode) * array_size);
                    free(trie.trie_node_array);
                    trie.trie_node_array = new_arr;
                    array_size *= 2;
                    node = trie.trie_node_array + offset;
                }
                node->children[chr] = &trie.trie_node_array[trie.num_nodes++];
            }
            struct TrieNode* next_node = node->children[chr];
            node = next_node;
        }
        node->may_be_leaf = true;
        node->index = i;
    }

    trie.trie_node_array = realloc(trie.trie_node_array, sizeof(struct TrieNode) * trie.num_nodes);
    return trie;
}