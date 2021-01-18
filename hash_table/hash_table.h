#ifndef OJIT_HASH_TABLE_H
#define OJIT_HASH_TABLE_H

struct HashTable {
    void* entries;
    size_t size;
    size_t used;
};


void init_hash_table(struct HashTable* table, size_t init_size);
bool hash_table_insert(struct HashTable* table, String key, uint64_t value);
bool hash_table_set(struct HashTable* table, String key, uint64_t value);
bool hash_table_get(struct HashTable* table, String key, uint64_t* value_ptr);

#endif //OJIT_HASH_TABLE_H
