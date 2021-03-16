#include <string.h>

#include "hash_table.h"


uint32_t hash_bytes(char* char_p, uint32_t length) {
    uint32_t hash = 2166136261;  // MAGIC

    for (size_t i = 0; i < length; i++) {
        hash ^= *(char_p++);
        hash *= 16777619;        // MAGIC
    }

    return hash == 0 ? 1 : hash;
}


uint32_t hash_ptr(void* ptr) {
    uint32_t hash = 2166136261;  // MAGIC

    for (size_t i = 0; i < 64; i += 8) {
        hash ^= ((uintptr_t) ptr >> i) & 0xFF;
        hash *= 16777619;        // MAGIC
    }

    return hash == 0 ? 1 : hash;
}


void init_hash_table(struct HashTable* table, MemCtx* mem) {
    table->first_block = lalist_grow(mem, NULL, NULL);
    table->last_entry = NULL;
    table->mem = mem;
    table->len = 0;
}


bool hash_table_insert(struct HashTable* table, HashKey key, uint64_t value) {
    size_t insert_index = key.hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key.cmp_obj) {
            if (key.cmp_obj == existing->key.cmp_obj) {
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
            table->len++;
            return true;
        }
    }
}


bool hash_table_set(struct HashTable* table, HashKey key, uint64_t value) {
    size_t insert_index = key.hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (true) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key.cmp_obj) {
            if (key.cmp_obj == existing->key.cmp_obj) {
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


bool hash_table_get(struct HashTable* table, HashKey key, uint64_t* value_ptr) {
    size_t insert_index = key.hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key.cmp_obj) {
            if (key.cmp_obj == existing->key.cmp_obj) {
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


bool hash_table_has(struct HashTable* table, HashKey key) {
    size_t insert_index = key.hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key.cmp_obj) {
            if (key.cmp_obj == existing->key.cmp_obj) {
                return true;
            }
            curr_block = curr_block->next;
        } else {
            return false;
        }
    }
    return false;
}

bool hash_table_remove(struct HashTable* table, HashKey key, TableEntry* next) {
    size_t insert_index = key.hash % (LALIST_BLOCK_SIZE / sizeof(TableEntry));

    LAList* curr_block = table->first_block;
    while (curr_block) {
        TableEntry* existing = lalist_get(curr_block, sizeof(TableEntry), insert_index);
        if (existing->key.cmp_obj) {
            if (key.cmp_obj == existing->key.cmp_obj) {
                existing->key.hash = 0;
                existing->key.cmp_obj = NULL;
                existing->value = 0;
                if (table->last_entry == existing) table->last_entry = existing->prev;
                if (next) next->prev = existing->prev;

                TableEntry* last_collided = NULL;
                curr_block = curr_block->next;
                while ((curr_block = curr_block->next) != NULL) {
                    TableEntry* collided = lalist_get(curr_block, sizeof(TableEntry), insert_index);
                    if (collided->key.cmp_obj) last_collided = collided;
                }
                existing->key = last_collided->key;
                existing->value = last_collided->value;
                existing->prev = last_collided->prev;
                table->len--;
                return true;
            }
            curr_block = curr_block->next;
        } else {
            return false;
        }
    }
    return false;
}