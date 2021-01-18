#include "ojit_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct s_StringTableSlot {
    String entry;
    bool slot_in_use;
};


bool init_string_table(struct StringTable* table, size_t init_slots) {
    table->slots = calloc(init_slots, sizeof(struct s_StringTableSlot));
    table->size = init_slots;
    table->used = 0;
    return true;
}


void string_table_resize(struct StringTable* table, size_t new_size) {
    if (new_size < table->used) {
        exit(-1);  // TODO programmer error
    }
    struct s_StringTableSlot* new_slots = calloc(new_size, sizeof(struct s_StringTableSlot));

    for (size_t i = 0; i < table->size; i++) {
        struct s_StringTableSlot slot = table->slots[i];
        if (slot.slot_in_use) {
            size_t new_index = slot.entry->hash % new_size;
            while (new_slots[new_index].slot_in_use) {
                new_index++;
                if (new_index == table->size) {
                    new_index = 0;
                }
            }
            new_slots[new_index].slot_in_use = true;
            new_slots[new_index].entry = slot.entry;
        }
    }
    free(table->slots);
    table->slots = new_slots;
    table->size = new_size;
}


// 32-bit FNV-1a
uint32_t hash_string(char* char_p, uint32_t length) {
    uint32_t hash = 2166136261;  // MAGIC

    for (size_t i = 0; i < length; i++) {
        hash ^= *(char_p++);
        hash *= 16777619;        // MAGIC
    }

    return hash == 0 ? 1 : hash;
}

bool string_equal(String a, String b) {
    return a == b;
}


bool mem_string_check_equal(String a, String b) {
    return a->hash == b->hash && a->length == b->length && (strncmp(a->start_ptr, b->start_ptr, a->length) == 0);
}


char null_term_buf[256];
char* null_terminate_string(String s) {
    memset(null_term_buf, 0, 256);
    memcpy(null_term_buf, s->start_ptr, s->length);
    return null_term_buf;
}


String string_table_add(struct StringTable* table, char* ptr, uint32_t length) {
    if (((double) table->used / table->size) > 0.75) {
        string_table_resize(table, table->size*2);
    }
    uint32_t hash = hash_string(ptr, length);
    String entry = malloc(sizeof(struct s_String));
    entry->start_ptr = ptr;
    entry->length = length;
    entry->hash = hash;

    size_t insert_index = hash % table->size;
    while (table->slots[insert_index].slot_in_use) {
        if (mem_string_check_equal(entry, table->slots[insert_index].entry)) {
            free(entry);
            return table->slots[insert_index].entry;
        }
        insert_index++;
        if (insert_index == table->size) {
            insert_index = 0;
        }
    }
    table->slots[insert_index].slot_in_use = true;
    table->slots[insert_index].entry = entry;
    return table->slots[insert_index].entry;
}


String string_table_insert(struct StringTable* table, char* ptr, uint32_t length) {
    if (((double) table->used / table->size) > 0.75) {
        string_table_resize(table, table->size*2);
    }
    uint32_t hash = hash_string(ptr, length);
    struct s_String* entry = malloc(sizeof(struct s_String));
    entry->start_ptr = ptr;
    entry->length = length;
    entry->hash = hash;

    size_t insert_index = hash % table->size;
    while (table->slots[insert_index].slot_in_use) {
        if (mem_string_check_equal(entry, table->slots[insert_index].entry)) {
            free(entry);
            printf("Attempted to insert a string into the string table, but it already exists. Use a different function");
            exit(-1);  // TODO programmer error
        }
        insert_index++;
        if (insert_index == table->size) {
            insert_index = 0;
        }
    }
    table->slots[insert_index].slot_in_use = true;
    table->slots[insert_index].entry = entry;
    return table->slots[insert_index].entry;
}



String read_file(struct StringTable* table, char* path) {
    FILE* file = fopen(path, "r"); // TODO check for null
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buf = malloc(file_size);

    size_t amount_read = fread(buf, sizeof(char), file_size, file);
    String s = string_table_add(table, buf, amount_read);
    fclose(file);
    return s;
}
