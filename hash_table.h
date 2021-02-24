#ifndef OJIT_HASH_TABLE_H
#define OJIT_HASH_TABLE_H

#include "ojit_string.h"
#include "ojit_mem.h"

struct HashTable {
    LAList* first_block;
    MemCtx* mem;
};

void init_hash_table(struct HashTable* table, MemCtx* mem);
bool hash_table_insert(struct HashTable* table, String key, uint64_t value);
bool hash_table_set(struct HashTable* table, String key, uint64_t value);
bool hash_table_get(struct HashTable* table, String key, uint64_t* value_ptr);

#endif //OJIT_HASH_TABLE_H
