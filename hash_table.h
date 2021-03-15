#ifndef OJIT_HASH_TABLE_H
#define OJIT_HASH_TABLE_H

#include "ojit_string.h"
#include "ojit_mem.h"

struct HashTable {
    LAList* first_block;
    struct s_TableEntry* last_entry;
    MemCtx* mem;
};

typedef struct s_TableEntry {
    String key;
    uint64_t value;
    struct s_TableEntry* prev;
} TableEntry;

void init_hash_table(struct HashTable* table, MemCtx* mem);
bool hash_table_insert(struct HashTable* table, String key, uint64_t value);
bool hash_table_set(struct HashTable* table, String key, uint64_t value);
bool hash_table_get(struct HashTable* table, String key, uint64_t* value_ptr);
bool hash_table_has(struct HashTable* table, String key);
bool hash_table_remove(struct HashTable* table, String key, TableEntry* next);

#endif //OJIT_HASH_TABLE_H
