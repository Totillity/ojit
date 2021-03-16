#ifndef OJIT_HASH_TABLE_H
#define OJIT_HASH_TABLE_H

#include "ojit_mem.h"

struct HashTable {
    LAList* first_block;
    struct s_TableEntry* last_entry;
    MemCtx* mem;
    uint64_t len;
};

typedef struct s_HashKey {
    uint32_t hash;
    void* cmp_obj;
} HashKey;

typedef struct s_TableEntry {
    HashKey key;
    uint64_t value;
    struct s_TableEntry* prev;
} TableEntry;

#define HASH_KEY(ptr) ((HashKey) {.hash=hash_ptr(ptr), .cmp_obj=(ptr)})
#define STRING_KEY(str) ((HashKey) {.hash=(str)->hash, .cmp_obj=(str)})

uint32_t hash_bytes(char* char_p, uint32_t length);
uint32_t hash_ptr(void* ptr);

void init_hash_table(struct HashTable* table, MemCtx* mem);
bool hash_table_insert(struct HashTable* table, HashKey key, uint64_t value);
bool hash_table_set(struct HashTable* table, HashKey key, uint64_t value);
bool hash_table_get(struct HashTable* table, HashKey key, uint64_t* value_ptr);
bool hash_table_has(struct HashTable* table, HashKey key);
bool hash_table_remove(struct HashTable* table, HashKey key, TableEntry* next);

#endif //OJIT_HASH_TABLE_H
