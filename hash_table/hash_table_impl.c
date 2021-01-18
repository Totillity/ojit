#include <stdlib.h>
#include <string.h>

#include "../string_tools/ojit_string.h"
#include "hash_table.h"


struct TableEntry {
    String key;
    uint64_t value;
    bool is_used;
};


void init_hash_table(struct HashTable* table, size_t init_size) {
    table->entries = calloc(init_size, sizeof(struct TableEntry));
    table->size = init_size;
    table->used = 0;
}


void hash_table_resize(struct HashTable* table, size_t new_size) {
    if (new_size < table->used) {
        exit(-1);  // TODO programmer error
    }
    struct TableEntry* entries = table->entries;
    struct TableEntry* new_entries = calloc(new_size, sizeof(struct TableEntry));

    for (size_t i = 0; i < table->size; i++) {
        struct TableEntry entry = entries[i];
        if (entry.is_used) {
            size_t new_index = entry.key->hash % new_size;
            while (new_entries[new_index].is_used) {
                new_index++;
                if (new_index == table->size) {
                    new_index = 0;
                }
            }
            new_entries[new_index] = entry;
        }
    }
    free(table->entries);
    table->entries = new_entries;
    table->size = new_size;
}


bool hash_table_insert(struct HashTable* table, String key, uint64_t value) {
    if (((double) table->used / table->size) > 0.75) {
        hash_table_resize(table, table->size*2);
    }

    struct TableEntry* entries = table->entries;

    size_t insert_index = key->hash % table->size;
    while (entries[insert_index].is_used) {
        if (string_equal(key, entries[insert_index].key)) {
            return false;
        }
        insert_index++;
        if (insert_index == table->size) {
            insert_index = 0;
        }
    }
    entries[insert_index].key = key;
    entries[insert_index].is_used = true;
    entries[insert_index].value = value;
    return true;
}


bool hash_table_set(struct HashTable* table, String key, uint64_t value) {
    size_t set_index = key->hash % table->size;
    struct TableEntry* entries = table->entries;
    struct TableEntry* entry = &entries[set_index];
    while (entry->is_used && entry->key->hash == key->hash) {
        if (string_equal(key, entry->key)) {
            entry->value = value;
            return true;
        }
        set_index++;
        if (set_index == table->size) {
            set_index = 0;
        }
        entry = &entries[set_index];
    }
    return false;
}


bool hash_table_get(struct HashTable* table, String key, uint64_t* value_ptr) {
    size_t get_index = key->hash % table->size;
    struct TableEntry* entries = table->entries;
    struct TableEntry entry = entries[get_index];
    while (entry.is_used && entry.key->hash == key->hash) {
        if (string_equal(key, entry.key)) {
            *value_ptr = entry.value;
            return true;
        }
        get_index++;
        if (get_index == table->size) {
            get_index = 0;
        }
        entry = entries[get_index];
    }
    return false;
}
