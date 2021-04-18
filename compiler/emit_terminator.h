#ifndef OJIT_EMIT_TERMINATOR_H
#define OJIT_EMIT_TERMINATOR_H

#include "registers.h"
#include "emit_instr.h"

void __attribute__((always_inline)) emit_return(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct ReturnIR* ret = &terminator->ir_return;
    asm_emit_ret(&state->writer);
    instr_assign_loc(ret->value, WRAP_REG(RAX), state);
    asm_emit_mov(WRAP_REG(RAX), GET_LOC(ret->value), &state->writer);
    asm_emit_pop_r64(RBP, &state->writer);
    asm_emit_mov_r64_r64(RSP, RBP, &state->writer);
}


bool vloc_list_contains(VLoc** list, uint32_t num_items, VLoc item) {
    for (int i = 0; i < num_items; i++) {
        if (list[i] && loc_equal(*list[i], item)) return true;
    }
    return false;
}

void __attribute__((always_inline)) resolve_defined_arguments(struct BlockIR* target, VLoc** swap_from, VLoc** swap_to, VLoc** target_locs, uint32_t* target_locs_index, struct AssemblerState* state) {
    FOREACH_INSTR(instr, target->first_instrs) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR) {
            struct ParameterIR* param = &instr->ir_parameter;
            if (param->base.refs == 0) continue;
            IRValue argument;
            hash_table_get(&state->block->variables, STRING_KEY(param->var_name), (uint64_t*) &argument);

            if (IS_ASSIGNED(GET_LOC(argument)) || INSTR_TYPE(argument) == ID_BLOCK_PARAMETER_IR) {
                instr_assign_loc(argument, param->entry_loc, state);
                VLoc* arg_loc = &GET_LOC(argument);
                VLoc* param_loc = &param->entry_loc;
                if (!IS_ASSIGNED(*param_loc)) {
                    *param_loc = *arg_loc;
                    if (vloc_list_contains(target_locs, target->num_params, *arg_loc)) {
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RAX))) {
                            *param_loc = WRAP_REG(RAX);
                        } else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RCX))) {
                            *param_loc = WRAP_REG(RCX);
                        }
                        else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RDX))) {
                            *param_loc = WRAP_REG(RDX);
                        }
                        else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R8))) {
                            *param_loc = WRAP_REG(R8);
                        }
                        else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R9))) {
                            *param_loc = WRAP_REG(R9);
                        }
                        else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R10))) {
                            *param_loc = WRAP_REG(R10);
                        }
                        else if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R11))) {
                            *param_loc = WRAP_REG(R11);
                        } else {
                            ojit_exit(57);
                        }
                    }
                }
                target_locs[(*target_locs_index)++] = param_loc;
                swap_from[instr->base.index] = arg_loc;
                swap_to[instr->base.index] = param_loc;
            }
        } else {
            break;
        }
    }
}

void __attribute__((always_inline)) resolve_undefined_arguments(struct BlockIR* target, VLoc** swap_from, VLoc** swap_to, VLoc** target_locs, uint32_t* target_locs_index, struct AssemblerState* state) {
    FOREACH_INSTR(instr, target->first_instrs) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR) {
            struct ParameterIR* param = &instr->ir_parameter;
            if (param->base.refs == 0) continue;
            IRValue argument;
            hash_table_get(&state->block->variables, STRING_KEY(param->var_name), (uint64_t*) &argument);

            if (!IS_ASSIGNED(GET_LOC(argument)) || INSTR_TYPE(argument) != ID_BLOCK_PARAMETER_IR) {
                instr_assign_loc(argument, param->entry_loc, state);
                VLoc* arg_loc = &GET_LOC(argument);
                VLoc* param_loc = &param->entry_loc;
                if (!IS_ASSIGNED(*param_loc)) {
                    *param_loc = *arg_loc;
                    if (vloc_list_contains(target_locs, target->num_params, *arg_loc)) {
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RAX))) {
                            *param_loc = WRAP_REG(RAX);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RCX))) {
                            *param_loc = WRAP_REG(RCX);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(RDX))) {
                            *param_loc = WRAP_REG(RDX);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R8))) {
                            *param_loc = WRAP_REG(R8);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R9))) {
                            *param_loc = WRAP_REG(R9);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R10))) {
                            *param_loc = WRAP_REG(R10);
                            goto done;
                        }
                        if (!vloc_list_contains(target_locs, target->num_params, WRAP_REG(R11))) {
                            *param_loc = WRAP_REG(R11);
                            goto done;
                        }
                        ojit_exit(57);
                    }
                }
                done:
                target_locs[(*target_locs_index)++] = param_loc;
                swap_from[instr->base.index] = arg_loc;
                swap_to[instr->base.index] = param_loc;
            }
        } else {
            break;
        }
    }
}

void __attribute__((always_inline)) resolve_branch(struct BlockIR* target, struct AssemblerState* state) {
    VLoc* swap_from[target->num_params];
    VLoc* swap_to[target->num_params];
    for (int i = 0; i < target->num_params; i++) {
        swap_from[i] = NULL;
        swap_to[i] = NULL;
    }

//    uint64_t target_locs = UINT64_MAX ^ (
//            (1 << RBX) |
//            (1 << NO_REG) |
//            (1 << SPILLED_REG) |
//            (1 << RSI) |
//            (1 << RDI) |
//            (1 << TMP_1_REG) |
//            (1 << TMP_2_REG)
//    );
    VLoc* target_locs[target->num_params]; for (int i = 0; i < target->num_params; i++) target_locs[i] = NULL;
    uint32_t target_locs_index = 0;

    resolve_defined_arguments(target, swap_from, swap_to, target_locs, &target_locs_index, state);
    resolve_undefined_arguments(target, swap_from, swap_to, target_locs, &target_locs_index, state);

    map_registers(swap_from, swap_to, target->num_params, &state->writer);
}

void __attribute__((always_inline)) emit_branch(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct BranchIR* branch = &terminator->ir_branch;

    asm_emit_jmp(branch->target->data, &state->writer);  // comeback to this later after I've stitched everything together

    resolve_branch(branch->target, state);
}

void __attribute__((always_inline)) emit_cbranch(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct CBranchIR* cbranch = &terminator->ir_cbranch;

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(cbranch->cond) == ID_CMP_IR) {
        if (!IS_ASSIGNED(GET_LOC(cbranch->cond))) {
            asm_emit_jcc(IF_NOT_ZERO, cbranch->true_target->data, &state->writer);
            resolve_branch(cbranch->true_target, state);
            asm_emit_jcc(INV_CMP(cbranch->cond->ir_cmp.cmp), cbranch->false_target->data, &state->writer);
            resolve_branch(cbranch->false_target, state);
            emit_cmp(cbranch->cond, state, false);
            return;
        }
    }
#endif

    asm_emit_jcc(IF_NOT_ZERO, cbranch->true_target->data, &state->writer);
    resolve_branch(cbranch->true_target, state);
    asm_emit_jcc(IF_ZERO, cbranch->false_target->data, &state->writer);
    resolve_branch(cbranch->false_target, state);

    enum Registers reg = postload_loc(&GET_LOC(cbranch->cond), WRAP_NONE(), state);
    asm_emit_test_r64_r64(reg, reg, &state->writer);
    load_loc(&GET_LOC(cbranch->cond), state);
}

void __attribute__((always_inline)) emit_terminator(union TerminatorIR* terminator_ir, struct AssemblerState* state) {
    switch (terminator_ir->ir_base.id) {
        case ID_RETURN_IR: emit_return(terminator_ir, state); break;
        case ID_BRANCH_IR: emit_branch(terminator_ir, state); break;
        case ID_CBRANCH_IR: emit_cbranch(terminator_ir, state); break;
        case ID_TERM_NONE:
        default:
            ojit_new_error();
            ojit_build_error_chars("Either Unimplemented or missing terminator: ID ");
            ojit_build_error_int(terminator_ir->ir_base.id);
            ojit_error();
            exit(-1);
    }
}
#endif //OJIT_EMIT_TERMINATOR_H
