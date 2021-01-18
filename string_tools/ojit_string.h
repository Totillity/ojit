#ifndef OJIT_OJIT_STRING_H
#define OJIT_OJIT_STRING_H

#include <stdint.h>
#include <stdbool.h>

struct StringTable {
    struct s_StringTableSlot* slots;
    size_t size;
    size_t used;
};

typedef struct s_String {
    char* start_ptr;
    uint32_t length;
    uint32_t hash;
}* String;

bool init_string_table(struct StringTable* table, size_t init_slots);
String string_table_add(struct StringTable* table, char* ptr, uint32_t length);

bool string_equal(String a, String b);
bool mem_string_check_equal(String a, String b);
char* null_terminate_string(String s);

String read_file(struct StringTable* table, char* path);

#endif //OJIT_OJIT_STRING_H
