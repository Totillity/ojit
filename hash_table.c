#include <string.h>

#include "ojit_string.h"
#include "hash_table.h"


void init_hash_table(struct HashTable* table, MemCtx* mem) {
    table->first_block = lalist_grow(mem, NULL, NULL);
    table->last_entry = NULL;
    table->mem = mem;
}


bool hash_table_insert(struct HashTable* table, String key, uint64_t value) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key) {
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
            existing->prev = table->last_entry;
            table->last_entry = existing;
            existing->key = key;
            existing->value = value;
            return true;
        }
    }
}


bool hash_table_set(struct HashTable* table, String key, uint64_t value) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key) {
            if (string_equal(key, existing->key)) {
                existing->key = key;
                existing->value = value;
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
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key) {
            if (string_equal(key, existing->key)) {
                *value_ptr = existing->value;
                return true;
            }
            curr_block = curr_block->next;
        } else {
            return false;
        }
    }
    return false;
}


bool hash_table_has(struct HashTable* table, String key) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key) {
            if (string_equal(key, existing->key)) {
                return true;
            }
            curr_block = curr_block->next;
        } else {
            return false;
        }
    }
    return false;
}

bool hash_table_remove(struct HashTable* table, String key, TableEntry* next) {
    size_t insert_index = key->hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key) {
            if (string_equal(key, existing->key)) {
                existing->key = NULL;
                existing->value = 0;
                if (table->last_entry == existing) table->last_entry = existing->prev;
                if (next) next->prev = existing->prev;

                TableEntry* last_collided = NULL;
                curr_block = curr_block->next;
                while ((curr_block = curr_block->next) != NULL) {
                    TableEntry* collided = lalist_get(curr_block, sizeof(TableEntry), insert_index);
                    if (collided->key) last_collided = collided;
                }
                existing->key = last_collided->key;
                existing->value = last_collided->value;
                existing->prev = last_collided->prev;

                return true;
            }
            curr_block = curr_block->next;
        } else {
            return false;
        }
    }
    return false;
}