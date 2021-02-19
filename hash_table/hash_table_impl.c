//#include <stdlib.h>
#include <string.h>

#include "../string_tools/ojit_string.h"
#include "hash_table.h"


struct TableEntry {
    String key;
    uint64_t value;
    bool is_used;
};


void init_hash_table(struct HashTable* table, MemCtx* mem) {
    table->first_block = lalist_grow(mem, NULL, NULL);
    table->mem = mem;
}


bool hash_table_insert(struct HashTable* table, String key, uint64_t value) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(struct TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        struct TableEntry* existing = lalist_get(curr_block, sizeof(struct TableEntry), insert_index);
        if (existing->is_used) {
            if (string_equal(key, existing->key)) {
                return false;
            } else {
                if (curr_block->next) {
                    curr_block = curr_block->next;
                } else {
                    curr_block = lalist_grow(table->mem, curr_block, NULL);
                }
            }
        } else {
            existing->key = key;
            existing->value = value;
            existing->is_used = true;
            return true;
        }
    }
}


bool hash_table_set(struct HashTable* table, String key, uint64_t value) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(struct TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        struct TableEntry* existing = lalist_get(curr_block, sizeof(struct TableEntry), insert_index);
        if (existing->is_used) {
            if (string_equal(key, existing->key)) {
                existing->key = key;
                existing->value = value;
                existing->is_used = true;
                return true;
            } else {
                if (curr_block->next) {
                    curr_block = curr_block->next;
                } else {
                    curr_block = lalist_grow(table->mem, curr_block, NULL);
                }
            }
        } else {
            return false;
        }
    }
}


bool hash_table_get(struct HashTable* table, String key, uint64_t* value_ptr) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(struct TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        struct TableEntry* existing = lalist_get(curr_block, sizeof(struct TableEntry), insert_index);
        if (existing->is_used) {
            if (string_equal(key, existing->key)) {
                *value_ptr = existing->value;
                return true;
            } else {
                if (curr_block->next) {
                    curr_block = curr_block->next;
                } else {
                    curr_block = lalist_grow(table->mem, curr_block, NULL);
                }
            }
        } else {
            return false;
        }
    }
}
