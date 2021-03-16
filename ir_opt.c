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

void ojit_optimize_params(struct FunctionIR* func) {
    FOREACH_REV(block, func->last_blocks, struct BlockIR) {
        bool was_used[block->num_instrs]; ojit_memset(was_used, false, block->num_instrs);
#define INDEX(instr_ptr) ((instr_ptr)->base.index)

        union TerminatorIR term = block->terminator;
        switch (term.ir_base.id) {
            case ID_RETURN_IR: {
                was_used[INDEX(term.ir_return.value)] = true;
                break;
            }
            case ID_BRANCH_IR: {
                FOREACH_INSTR(param, term.ir_branch.target->first_instrs) {
                    if (param->base.id == ID_BLOCK_PARAMETER_IR) {
                        String var_name = param->ir_parameter.var_name;
                        if (var_name) {
                            Instruction* instr_ptr; hash_table_get(&block->variables, STRING_KEY(var_name), (uint64_t*) &instr_ptr);
                            was_used[INDEX(instr_ptr)] = true;
                        }
                    } else {
                        break;
                    }
                }
                break;
            }
            case ID_CBRANCH_IR: {
                FOREACH_INSTR(param, term.ir_cbranch.true_target->first_instrs) {
                    if (param->base.id == ID_BLOCK_PARAMETER_IR) {
                        String var_name = param->ir_parameter.var_name;
                        if (var_name) {
                            Instruction* instr_ptr; hash_table_get(&block->variables, STRING_KEY(var_name), (uint64_t*) &instr_ptr);
                            was_used[INDEX(instr_ptr)] = true;
                        }
                    } else {
                        break;
                    }
                }
                FOREACH_INSTR(param1, term.ir_cbranch.false_target->first_instrs) {
                    if (param1->base.id == ID_BLOCK_PARAMETER_IR) {
                        String var_name = param1->ir_parameter.var_name;
                        if (var_name) {
                            Instruction* instr_ptr; hash_table_get(&block->variables, STRING_KEY(var_name), (uint64_t*) &instr_ptr);
                            was_used[INDEX(instr_ptr)] = true;
                        }
                    } else {
                        break;
                    }
                }
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
#undef INDEX
    }
}

void ojit_optimize_func(struct FunctionIR* func, struct GetFunctionCallback callbacks) {
    struct OptState state = {.callbacks = callbacks};

    FOREACH(block, func->first_blocks, struct BlockIR) {
        ojit_optimize_block(block, &state);
    }

    ojit_optimize_params(func);
}