#include "ojit_trie.h"


#include <stdlib.h>
#include <string.h>


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