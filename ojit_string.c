#include "ojit_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 32-bit FNV-1a
uint32_t hash_string(char* char_p, uint32_t length) {
    uint32_t hash = 2166136261;  // MAGIC

    for (size_t i = 0; i < length; i++) {
        hash ^= *(char_p++);
        hash *= 16777619;        // MAGIC
    }

    return hash == 0 ? 1 : hash;
}


bool init_string_table(struct StringTable* table, MemCtx* mem) {
    table->first_block = lalist_grow(mem, NULL, NULL);
    table->mem = mem;
    table->null_string.start_ptr = (char*) &table->null_string;
    table->null_string.length = 0;
    table->null_string.hash = hash_string("", 0);
    return true;
}

bool string_equal(String a, String b) {
    return a == b;
}


String string_table_add(struct StringTable* table, char* ptr, uint32_t length) {
    if (length == 0) {
        return &table->null_string;
    }
    uint32_t hash = hash_string(ptr, length);
    size_t insert_index = hash % (LALIST_BLOCK_SIZE / sizeof(struct s_StringRecord));

    LAList* curr_block = table->first_block;
    while (true) {
        struct s_StringRecord* existing = lalist_get(curr_block, sizeof(struct s_StringRecord), insert_index);
        if (existing->length != 0) {
            if (existing->hash == hash && existing->length == length && (strncmp(existing->start_ptr, ptr, existing->length) == 0)) {
                return existing;
            } else {
                if (curr_block->next) {
                    curr_block = curr_block->next;
                } else {
                    curr_block = lalist_grow(table->mem, curr_block, NULL);
                }
            }
        } else {
            existing->start_ptr = ptr;
            existing->length = length;
            existing->hash = hash;
            return existing;
        }
    }
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
//    free(buf);
    return s;
}
