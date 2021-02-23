#include "ojit_mem.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ojit_err.h"

#define OJIT_ARENA_SIZE (1024)

typedef struct s_MemArena {
    uint8_t* curr_ptr;
    uint8_t* end_ptr;
    struct s_MemArena* next_arena;
    struct s_MemArena* prev_arena;
    uint8_t mem[];
} MemArena;

struct s_OJITMemCtx {
    MemArena* curr_arena;
};

void mem_ctx_new_arena(MemCtx* ctx) {
    MemArena* arena = malloc(sizeof(struct s_MemArena) + OJIT_ARENA_SIZE);
    arena->next_arena = NULL;
    arena->prev_arena = ctx->curr_arena;
    if (ctx->curr_arena) ctx->curr_arena->next_arena = arena;
    ctx->curr_arena = arena;

    arena->curr_ptr = arena->mem;
    arena->end_ptr = &arena->mem[OJIT_ARENA_SIZE];
}

MemCtx* create_mem_ctx() {
    MemCtx* ctx = malloc(sizeof(struct s_OJITMemCtx));
    ctx->curr_arena = NULL;
    mem_ctx_new_arena(ctx);

    return ctx;
}

void destroy_mem_ctx(MemCtx* ctx) {
    MemArena* curr_arena = ctx->curr_arena;
    MemArena* next_arena = NULL;

    while (curr_arena) {
        next_arena = curr_arena->next_arena;
        free(curr_arena);
        curr_arena = next_arena;
    }

    free(ctx);
}

void* ojit_alloc(MemCtx* ctx, size_t size) {
    if (size > OJIT_ARENA_SIZE) {
        ojit_new_error();
        ojit_build_error_chars("Attempted to allocate a size larger than the size of an Arena");
        ojit_error();
        exit(-1);
    }
    if (ctx->curr_arena->curr_ptr + size >= ctx->curr_arena->end_ptr) {
        mem_ctx_new_arena(ctx);
    }
    void* ptr = ctx->curr_arena->curr_ptr;
    ctx->curr_arena->curr_ptr += size;
    ojit_memset(ptr, 0, size);
    return ptr;
}

LAList* lalist_grow(MemCtx* mem, LAList* prev, LAList* next) {
    LAList* node = ojit_alloc(mem, sizeof(LAList));
    if (prev) prev->next = node;
    if (next) next->prev = node;
    node->prev = prev;
    node->next = next;
    node->len = 0;
    return node;
}

void* lalist_add(LAList* lalist, size_t item_size) {
    if (lalist->len + item_size > LALIST_BLOCK_SIZE) {
        return NULL;
    }
    void* item_ptr = lalist->mem + lalist->len;
    lalist->len += item_size;
    return item_ptr;
}

void* lalist_get_last(LAList* lalist) {
    return &lalist->mem[lalist->len];
}

void* lalist_get(LAList* lalist, size_t item_size, size_t index) {
    return &lalist->mem[item_size * index];
}

void lalist_init_iter(LAListIter* iter, LAList* lalist, size_t item_size) {
    iter->curr_list = lalist;
    iter->item_size = item_size;
    iter->curr_index = 0;
}

void lalist_iter_position(LAListIter* iter, size_t index) {
    iter->curr_index = index;
}

void* lalist_iter_next(LAListIter* iter) {
    if (iter->curr_list == NULL) return NULL;
    void* item = &iter->curr_list->mem[iter->curr_index];
    iter->curr_index += iter->item_size;
    if (iter->curr_index >= LALIST_BLOCK_SIZE) {
        iter->curr_list = iter->curr_list->next;
        iter->curr_index = 0;
    }
    return item;
}

void* lalist_iter_prev(LAListIter* iter) {
    if (iter->curr_list == NULL) return NULL;
    void* item = &iter->curr_list->mem[iter->curr_index];
    if (iter->curr_index < iter->item_size) {
        iter->curr_list = iter->curr_list->prev;
        if (iter->curr_list) iter->curr_index = iter->curr_list->len - iter->item_size;
    } else {
        iter->curr_index -= iter->item_size;
    }
    return item;
}

void ojit_memcpy(void* dest, void* src, size_t size) {
    memcpy(dest, src, size);
}

void ojit_memset(void* dest, uint8_t val, size_t num) {
    memset(dest, val, num);
}
#undef OJIT_ARENA_SIZE