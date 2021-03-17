#include "ir_opt.h"

struct OptState {
    struct GetFunctionCallback callbacks;
};

#define TYPE_OF(instr) ((instr)->base.id)

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

    bool a_is_int = TYPE_OF(add_ir->a) == ID_INT_IR;
    bool b_is_int = TYPE_OF(add_ir->b) == ID_INT_IR;

    if (a_is_int && b_is_int) {
        replace_instr_int(instr, add_ir->a->ir_int.constant + add_ir->b->ir_int.constant);
        disable_instr(add_ir->a);
        disable_instr(add_ir->b);
    } else if (a_is_int && TYPE_OF(add_ir->b) == ID_ADD_IR) {
        struct AddIR* inner_add = &add_ir->b->ir_add;
        uint32_t outer_const = add_ir->a->ir_int.constant;
        if (TYPE_OF(inner_add->a) == ID_INT_IR || TYPE_OF(inner_add->b) == ID_INT_IR) {
            uint32_t inner_const;
            Instruction* inner_val;
            if (TYPE_OF(inner_add->a) == ID_INT_IR) {
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
    } else if (b_is_int && TYPE_OF(add_ir->a) == ID_ADD_IR) {
        struct AddIR* inner_add = &add_ir->a->ir_add;
        uint32_t outer_const = add_ir->b->ir_int.constant;
        if (TYPE_OF(inner_add->a) == ID_INT_IR || TYPE_OF(inner_add->b) == ID_INT_IR) {
            uint32_t inner_const;
            Instruction* inner_val;
            if (TYPE_OF(inner_add->a) == ID_INT_IR) {
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

void ojit_peephole_optimizer(struct BlockIR* block, struct OptState* state) {
    (void) state;
    FOREACH_INSTR(instr, block->first_instrs) {
        switch (TYPE_OF(instr)) {
            case ID_ADD_IR: optimize_add_ir(instr); break;
            default: break;
        }
    }
}

void ojit_optimize_block(struct BlockIR* block, struct OptState* state) {
    ojit_peephole_optimizer(block, state);
}

void ojit_optimize_params_branch(struct BlockIR* target, struct BlockIR* block, bool* was_used) {
    FOREACH_INSTR(param, target->first_instrs) {
        if (param->base.id == ID_BLOCK_PARAMETER_IR) {
            String var_name = param->ir_parameter.var_name;
            if (var_name) {
                Instruction* instr_ptr; hash_table_get(&block->variables, STRING_KEY(var_name), (uint64_t*) &instr_ptr);
                was_used[instr_ptr->base.index] = true;
            }
        } else {
            break;
        }
    }
}

void ojit_optimize_params(struct FunctionIR* func) {
    struct BlockIR* block = func->last_block;
    while (block) {
        bool was_used[block->num_instrs]; ojit_memset(was_used, false, block->num_instrs);
#define INDEX(instr_ptr) ((instr_ptr)->base.index)

        union TerminatorIR term = block->terminator;
        switch (term.ir_base.id) {
            case ID_RETURN_IR: {
                was_used[INDEX(term.ir_return.value)] = true;
                break;
            }
            case ID_BRANCH_IR: {
                ojit_optimize_params_branch(term.ir_branch.target, block, was_used);
                break;
            }
            case ID_CBRANCH_IR: {
                ojit_optimize_params_branch(term.ir_cbranch.true_target, block, was_used);
                ojit_optimize_params_branch(term.ir_cbranch.false_target, block, was_used);
                break;
            }
            default: {
                exit(-1);
            }
        }

        FOREACH_REV(instr, block->last_instrs, Instruction) {
            switch (instr->base.id) {
                case ID_BLOCK_PARAMETER_IR: {
                    if (!was_used[INDEX(instr)]) {
                        instr->ir_parameter.var_name = NULL;
                    }
                    break;
                }
                case ID_INT_IR: {
                    break;
                }
                case ID_GLOBAL_IR: {
                    break;
                }
                case ID_ADD_IR: {
                    if (was_used[INDEX(instr)]) {
                        was_used[INDEX(instr->ir_add.a)] = true;
                        was_used[INDEX(instr->ir_add.b)] = true;
                    }
                    break;
                }
                case ID_SUB_IR: {
                    if (was_used[INDEX(instr)]) {
                        was_used[INDEX(instr->ir_sub.a)] = true;
                        was_used[INDEX(instr->ir_sub.b)] = true;
                    }
                    break;
                }
                case ID_CMP_IR: {
                    if (was_used[INDEX(instr)]) {
                        was_used[INDEX(instr->ir_sub.a)] = true;
                        was_used[INDEX(instr->ir_sub.b)] = true;
                    }
                    break;
                }
                case ID_CALL_IR: {
                    FOREACH(arg, instr->ir_call.arguments, IRValue) {
                        was_used[INDEX(*arg)] = true;
                    }
                    break;
                }
                case ID_INSTR_NONE: {
                    exit(-1);
                }
            }
        }
        block = block->prev_block;
#undef INDEX
    }
}

void ojit_optimize_func(struct FunctionIR* func, struct GetFunctionCallback callbacks) {
    struct OptState state = {.callbacks = callbacks};

    struct BlockIR* block = func->first_block;
    while (block) {
        ojit_optimize_block(block, &state);
        block = block->next_block;
    }

    ojit_optimize_params(func);
}