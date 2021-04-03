#include "ir_opt.h"

struct OptState {
    struct GetFunctionCallback callbacks;
};

enum FoldStep {
    CONTINUE_FOLD,
    REPEAT_FOLD,
};

#define AS_INSTR(instr) ((Instruction*) (instr))

void disable_instr(Instruction* instr) {
    (void) instr;
}
void replace_instr_int(Instruction* instr, uint32_t constant) {
    instr->base.id = ID_INT_IR;
    instr->ir_int.constant = constant;
}
void replace_instr_add(Instruction* instr, Instruction* a, Instruction* b) {
    instr->base.id = ID_ADD_IR;
    instr->ir_add.a = a;
    instr->ir_add.b = b;
}

void optimize_add_ir(Instruction* instr) {
    struct AddIR* add_ir = &instr->ir_add;

    bool a_is_int = INSTR_TYPE(add_ir->a) == ID_INT_IR;
    bool b_is_int = INSTR_TYPE(add_ir->b) == ID_INT_IR;

    if (a_is_int && b_is_int) {
        replace_instr_int(instr, add_ir->a->ir_int.constant + add_ir->b->ir_int.constant);
        disable_instr(add_ir->a);
        disable_instr(add_ir->b);
    } else if (a_is_int && INSTR_TYPE(add_ir->b) == ID_ADD_IR) {
        struct AddIR* inner_add = &add_ir->b->ir_add;
        uint32_t outer_const = add_ir->a->ir_int.constant;
        if (INSTR_TYPE(inner_add->a) == ID_INT_IR || INSTR_TYPE(inner_add->b) == ID_INT_IR) {
            uint32_t inner_const;
            Instruction* inner_val;
            if (INSTR_TYPE(inner_add->a) == ID_INT_IR) {
                inner_const = inner_add->a->ir_int.constant;
                inner_val = inner_add->b;
                disable_instr(inner_add->a);
            } else {
                inner_const = inner_add->b->ir_int.constant;
                inner_val = inner_add->a;
                disable_instr(inner_add->b);
            }
            uint32_t new_constant = outer_const + inner_const;
            disable_instr(add_ir->b);
            replace_instr_int(add_ir->a, new_constant);
            replace_instr_add((Instruction*) add_ir, add_ir->a, inner_val);
        }
    } else if (b_is_int && INSTR_TYPE(add_ir->a) == ID_ADD_IR) {
        struct AddIR* inner_add = &add_ir->a->ir_add;
        uint32_t outer_const = add_ir->b->ir_int.constant;
        if (INSTR_TYPE(inner_add->a) == ID_INT_IR || INSTR_TYPE(inner_add->b) == ID_INT_IR) {
            uint32_t inner_const;
            Instruction* inner_val;
            if (INSTR_TYPE(inner_add->a) == ID_INT_IR) {
                inner_const = inner_add->a->ir_int.constant;
                inner_val = inner_add->b;
                disable_instr(inner_add->a);
            } else {
                inner_const = inner_add->b->ir_int.constant;
                inner_val = inner_add->a;
                disable_instr(inner_add->b);
            }
            uint32_t new_constant = outer_const + inner_const;
            disable_instr(add_ir->a);
            replace_instr_int(add_ir->b, new_constant);
            replace_instr_add((Instruction*) add_ir, inner_val, add_ir->b);
        }
    }
}

#define MATCH_ADD(a_type, b_type, func) \
    if (INSTR_TYPE(instr) == ID_ADD_IR && INSTR_TYPE(instr->ir_add.a) == a_type && INSTR_TYPE(instr->ir_add.b) == b_type) { \
        next_step = func((void*) instr, (void*) instr->ir_add.a, (void*) instr->ir_add.b); \
    } else

#define DEFAULT(func) {next_step = func(instr);}

enum FoldStep fold_add_int_int(struct AddIR* instr, struct IntIR* a, struct IntIR* b) {
    replace_instr_int(AS_INSTR(instr), a->constant + b->constant);
    return REPEAT_FOLD;
}

void fold_communtative_add(struct AddIR* instr, struct AddIR* inner_add, struct IntIR* outer_const, struct IntIR* inner_const, Instruction* val) {
    uint32_t new_const = outer_const->constant + inner_const->constant;
    replace_instr_int((Instruction*) outer_const, new_const);
    replace_instr_add((Instruction*) instr, (Instruction*) outer_const, val);
    DEC_INSTR(inner_add);
    DEC_INSTR(inner_const);
}

enum FoldStep fold_add_int_add(struct AddIR* instr, struct IntIR* a, struct AddIR* b) {
    if (INSTR_REF(b) == 1 && (INSTR_TYPE(b->a) == ID_INT_IR || INSTR_TYPE(b->b) == ID_INT_IR)) {
        Instruction* val;
        struct IntIR* inner_const;
        if (INSTR_TYPE(b->a) == ID_INT_IR) {
            val = b->b;
            inner_const = (struct IntIR*) b->a;
        } else {
            val = b->a;
            inner_const = (struct IntIR*) b->b;
        }
        fold_communtative_add(instr, b, a, inner_const, val);
        return REPEAT_FOLD;
    }
    return CONTINUE_FOLD;
}

enum FoldStep fold_add_add_int(struct AddIR* instr, struct AddIR* a, struct IntIR* b) {
    if (INSTR_REF(a) == 1 && (INSTR_TYPE(a->a) == ID_INT_IR || INSTR_TYPE(a->b) == ID_INT_IR)) {
        Instruction* val;
        struct IntIR* inner_const;
        if (INSTR_TYPE(a->a) == ID_INT_IR) {
            val = a->b;
            inner_const = (struct IntIR*) a->a;
        } else {
            val = a->a;
            inner_const = (struct IntIR*) a->b;
        }
        fold_communtative_add(instr, a, b, inner_const, val);
        return REPEAT_FOLD;
    }
    return CONTINUE_FOLD;
}

enum FoldStep fold_default(Instruction* instr) {
    (void) instr;
    return CONTINUE_FOLD;
}

void ojit_peephole_optimizer(struct BlockIR* block, struct OptState* opt_state) {
    (void) opt_state;

    bool was_used[block->num_instrs];
    for (int i = 0; i < block->num_instrs; i++) was_used[i] = false;

    FOREACH(instr, block->first_instrs, Instruction) {
        enum FoldStep next_step = REPEAT_FOLD;
        while (next_step == REPEAT_FOLD) {
            MATCH_ADD(ID_INT_IR, ID_INT_IR, fold_add_int_int)
            MATCH_ADD(ID_INT_IR, ID_ADD_IR, fold_add_int_add)
            MATCH_ADD(ID_ADD_IR, ID_INT_IR, fold_add_add_int)
            DEFAULT(fold_default)
        }
    }
}

void ojit_optimize_block(struct BlockIR* block, struct OptState* state) {
    ojit_peephole_optimizer(block, state);
}

void ojit_optimize_params_branch(struct BlockIR* target, struct BlockIR* block) {
    FOREACH_INSTR(param, target->first_instrs) {
        if (param->base.id == ID_BLOCK_PARAMETER_IR) {
            String var_name = param->ir_parameter.var_name;
            if (var_name) {
                Instruction* instr_ptr;
                hash_table_get(&block->variables, STRING_KEY(var_name), (uint64_t*) &instr_ptr);
                if (param->base.refs > 0) {
                } else {
                    DEC_INSTR(instr_ptr);
                }
            }
        } else {
            break;
        }
    }
}

void ojit_optimize_params(struct FunctionIR* func) {
    struct BlockIR* block = func->last_block;
    while (block) {
        union TerminatorIR term = block->terminator;
        switch (term.ir_base.id) {
            case ID_BRANCH_IR: {
                ojit_optimize_params_branch(term.ir_branch.target, block);
                break;
            }
            case ID_CBRANCH_IR: {
                ojit_optimize_params_branch(term.ir_cbranch.true_target, block);
                ojit_optimize_params_branch(term.ir_cbranch.false_target, block);
                break;
            }
            default:
                break;
        }
        block = block->prev_block;
    }
}

void ojit_optimize_func(struct FunctionIR* func, struct GetFunctionCallback callbacks) {
    struct OptState state = {.callbacks = callbacks};

    struct BlockIR* block = func->first_block;
    while (block) {
//        ojit_optimize_block(block, &state);
        block = block->next_block;
    }

    ojit_optimize_params(func);
}