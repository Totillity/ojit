#include "ojit_state.h"
#include "asm_ir.h"


void init_block(struct BlockIR* block, size_t block_num, MemCtx* ctx) {
    block->first_instrs = block->last_instrs = lalist_grow(ctx, NULL, NULL);
    block->terminator.ir_base.id = ID_TERM_NONE;
    init_hash_table(&block->variables, ctx);
    block->block_num = block_num;
}

struct FunctionIR* create_function(String name, MemCtx* ctx) {
    struct FunctionIR* function = ojit_alloc(ctx, sizeof(struct FunctionIR));
    function->first_blocks = function->last_blocks = lalist_grow(ctx, NULL, NULL);
    function->name = name;
    function->num_blocks = 0;

    function->compiled = NULL;

    function_add_block(function, ctx);
    return function;
}

struct BlockIR* function_add_block(struct FunctionIR* func, MemCtx* ctx) {
    if (func->last_blocks->len + sizeof(struct BlockIR) > LALIST_BLOCK_SIZE) {
        func->last_blocks = lalist_grow(ctx, func->last_blocks, NULL);
    }
    struct BlockIR* block = lalist_add(func->last_blocks, sizeof(struct BlockIR));
    init_block(block, func->num_blocks++, ctx);
    return block;
}
