#ifndef OJIT_OJIT_MEM_H
#define OJIT_OJIT_MEM_H

#include <stdint.h>

#define LALIST_BLOCK_SIZE (1024)

typedef struct s_OJITMemCtx MemCtx;
MemCtx* create_mem_ctx();
void destroy_mem_ctx(MemCtx* ctx);

void* ojit_alloc(MemCtx* ctx, size_t size);

typedef struct s_LAList {
    uint8_t mem[LALIST_BLOCK_SIZE];
    size_t len;
    struct s_LAList* prev;
    struct s_LAList* next;
} LAList;  // linked array list

LAList* lalist_grow(MemCtx* mem, LAList* prev, LAList* next);
void* lalist_add(LAList* lalist, size_t item_size);
void* lalist_get(LAList* lalist, size_t item_size, size_t index);
void* lalist_get_last(LAList* lalist);

typedef struct s_LAListIter {
    LAList* curr_list;
    size_t curr_index;
    size_t item_size;
} LAListIter;
void lalist_init_iter(LAListIter* iter, LAList* lalist, size_t item_size);
void lalist_iter_position(LAListIter* iter, size_t index);
void* lalist_iter_next(LAListIter* iter);
void* lalist_iter_prev(LAListIter* iter);

void ojit_memcpy(void* dest, void* src, size_t size);
void ojit_memset(void* dest, uint8_t val, size_t num);

#endif //OJIT_OJIT_MEM_H