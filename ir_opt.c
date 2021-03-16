#include "ir_opt.h"

struct OptState {
    struct GetFunctionCallback callbacks;
};

#define TYPE_OF(instr) ((instr)->base.id)
#define INT_CONST(instr) ((instr)->ir_int.constant)

void optimize_add_ir(Instruction* instr) {
    struct AddIR* add_ir = &instr->ir_add;
    if (TYPE_OF(add_ir->b) == ID_INT_IR) {
        if (TYPE_OF(add_ir->a) == ID_INT_IR) {
            uint32_t sum = add_ir->a->ir_int.constant + add_ir->b->ir_int.constant;
            instr->base.id = ID_INT_IR;
            instr->ir_int.constant = sum;
#ifdef OJIT_READABLE_IR
            instr->ir_add.a->base.is_disabled = true;
            instr->ir_add.b->base.is_disabled = true;
#endif
        } else if (TYPE_OF(add_ir->a) == ID_ADD_IR) {
            struct AddIR* inner_add = &add_ir->a->ir_add;
            if (TYPE_OF(inner_add->a) == ID_INT_IR) {
                uint32_t sum = INT_CONST(inner_add->a) + INT_CONST(add_ir->b);
                add_ir->a = inner_add->b;
                INT_CONST(add_ir->b) = sum;
#ifdef OJIT_READABLE_IR
                inner_add->base.is_disabled = true;
                inner_add->a->base.is_disabled = true;
#endif
            } else if (TYPE_OF(inner_add->b) == ID_INT_IR) {
                uint32_t sum = INT_CONST(inner_add->b) + INT_CONST(add_ir->b);
                add_ir->a = inner_add->a;
                INT_CONST(add_ir->b) = sum;
#ifdef OJIT_READABLE_IR
                inner_add->base.is_disabled = true;
                inner_add->b->base.is_disabled = true;
#endif
            }
        }
    } else if (TYPE_OF(add_ir->a) == ID_INT_IR) {
        if (TYPE_OF(add_ir->b) == ID_ADD_IR) {
            struct AddIR* inner_add = &add_ir->b->ir_add;
            if (TYPE_OF(inner_add->a) == ID_INT_IR) {
                uint32_t sum = INT_CONST(inner_add->a) + INT_CONST(add_ir->a);
                add_ir->b = inner_add->b;
                INT_CONST(add_ir->a) = sum;
#ifdef OJIT_READABLE_IR
                inner_add->base.is_disabled = true;
                inner_add->a->base.is_disabled = true;
#endif
            } else if (TYPE_OF(inner_add->b) == ID_INT_IR) {
                uint32_t sum = INT_CONST(inner_add->b) + INT_CONST(add_ir->a);
                add_ir->b = inner_add->a;
                INT_CONST(add_ir->a) = sum;
#ifdef OJIT_READABLE_IR
                inner_add->base.is_disabled = true;
                inner_add->b->base.is_disabled = true;
#endif
            }
        }
    }
}

void ojit_optimize_block(struct BlockIR* block, struct OptState* state) {
    (void) state;
    LAListIter instr_iter;
    lalist_init_iter(&instr_iter, block->first_instrs, sizeof(Instruction));
    Instruction* instr = lalist_iter_next(&instr_iter);
    while (instr) {
        switch (TYPE_OF(instr)) {
            case ID_ADD_IR: optimize_add_ir(instr); break;
            default: break;
        }
        instr = lalist_iter_next(&instr_iter);
    }
}

void ojit_optimize_func(struct FunctionIR* func, struct GetFunctionCallback callbacks) {
#ifdef OJIT_OPTIMIZATIONS
    struct OptState state = {.callbacks = callbacks};

    LAListIter block_iter;
    lalist_init_iter(&block_iter, func->first_blocks, sizeof(struct BlockIR));
    struct BlockIR* block = lalist_iter_next(&block_iter);
    while (block) {
        ojit_optimize_block(block, &state);
        block = lalist_iter_next(&block_iter);
    }
#else
    return;
#endif
}