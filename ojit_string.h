#ifndef OJIT_OJIT_STRING_H
#define OJIT_OJIT_STRING_H

#include <stdint.h>
#include <stdbool.h>
#include "ojit_mem.h"

typedef struct s_StringRecord {
    char* start_ptr;
    uint32_t length;
    uint32_t hash;
}* String;

struct StringTable {
    LAList* first_block;
    MemCtx* mem;
    struct s_StringRecord null_string;
};

bool init_string_table(struct StringTable* table, MemCtx* mem);
String string_table_add(struct StringTable* table, char* ptr, uint32_t length);

bool string_equal(String a, String b);

String read_file(struct StringTable* table, char* path);

#endif //OJIT_OJIT_STRING_H
