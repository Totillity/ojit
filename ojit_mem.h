#ifndef OJIT_OJIT_MEM_H
#define OJIT_OJIT_MEM_H

#include <stdint.h>
#include <stdbool.h>

#define FOREACH(iter_var, iter_over, type) LAListIter iter_##iter_var; \
                                           lalist_init_iter(&iter_##iter_var, (iter_over), sizeof(type)); \
                                           type* iter_var; \
                                           while (((iter_var) = lalist_iter_next(&iter_##iter_var)) != NULL)

#define LALIST_BLOCK_SIZE (500)

typedef struct s_OJITMemCtx MemCtx;
MemCtx* create_mem_ctx();
void destroy_mem_ctx(MemCtx* ctx);

void* ojit_alloc(MemCtx* ctx, size_t size);

typedef struct s_LAList {
    uint8_t mem[LALIST_BLOCK_SIZE];
    size_t len;
    MemCtx* ctx;
    struct s_LAList* prev;
    struct s_LAList* next;
} LAList;  // linked array list

LAList* lalist_grow(MemCtx* mem, LAList* prev, LAList* next);
bool lalist_can_add(LAList* lalist, size_t item_size);
void* lalist_grow_add(LAList** lalist_ptr, size_t item_size);
//void* lalist_add(LAList* lalist, size_t item_size);
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
