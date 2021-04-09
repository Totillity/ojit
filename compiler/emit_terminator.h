#ifndef OJIT_EMIT_TERMINATOR_H
#define OJIT_EMIT_TERMINATOR_H

#include "registers.h"
#include "emit_instr.h"

void __attribute__((always_inline)) emit_return(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct ReturnIR* ret = &terminator->ir_return;
    Register64 value_reg = instr_fetch_reg(ret->value, RAX, state);
    asm_emit_byte(0xC3, &state->writer);
    if (value_reg != RAX) {
        asm_emit_mov_r64_r64(RAX, value_reg, &state->writer);
    }
}

Register64 __attribute__((always_inline)) find_target_reg(bool* target_registers, Register64 suggestion, struct AssemblerState* state) {
    if (!target_registers[suggestion]) return suggestion;
    for (Register64 reg = 0; reg < 16; reg++) {
        if (!target_registers[reg] && !state->used_registers[reg]) {
            return reg;
        }
    }
    ojit_new_error();
    ojit_build_error_chars("Too many registers used concurrently");
    ojit_exit(-1);
    exit(-1);
}

void __attribute__((always_inline)) resolve_branch(struct BlockIR* target, struct AssemblerState* state) {
    bool target_registers[16] = {
            [RAX] = false,
            [RCX] = false,
            [RDX] = false,
            [RBX] = false,
            [NO_REG]      = true,
            [SPILLED_REG] = true,
            [RSI] = false,
            [RDI] = false,
            [R8]  = false,
            [R9]  = false,
            [R10] = false,
            [R11] = false,
            [TMP_1_REG]   = true,
            [TMP_2_REG]   = true,
            [R14] = false,
            [R15] = false,
    };

    FOREACH_INSTR(instr, target->first_instrs) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR) {
            struct ParameterIR* param = &instr->ir_parameter;
            if (param->base.refs == 0) continue;
            IRValue argument;
            hash_table_get(&state->block->variables, STRING_KEY(param->var_name), (uint64_t*) &argument);

            Register64 param_reg = param->entry_reg;
            Register64 argument_reg = GET_REG(argument);

            if (IS_ASSIGNED(param_reg)) {
                if (REG_IS_MARKED(param_reg)) {
                    // TODO oh god not again
                    ojit_new_error();
                    ojit_build_error_chars("something here also used the register the argument needs to go into");
                    ojit_exit(-1);
                    exit(-1);
                }
            } else {
                if (!IS_ASSIGNED(argument_reg) && INSTR_TYPE(argument) == ID_BLOCK_PARAMETER_IR && argument->ir_parameter.entry_reg != NO_REG) {
//                    argument_reg = ;
                    argument_reg = instr_fetch_reg(argument, argument->ir_parameter.entry_reg, state);
//                    instr_assign_reg(argument, argument_reg);
//                    mark_register(argument_reg, state);
                }
                param_reg = find_target_reg(target_registers, argument_reg, state);
                target_registers[param_reg] = true;
                param->entry_reg = param_reg;
            }
            if (!IS_ASSIGNED(argument_reg)) {
                argument_reg = instr_fetch_reg(argument, param_reg, state);
            }
            asm_emit_mov_r64_r64(param_reg, argument_reg, &state->writer);
        } else {
            break;
        }
    }
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
        if (!IS_ASSIGNED(GET_REG(cbranch->cond))) {
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

    Register64 value_reg = instr_fetch_reg(cbranch->cond, NO_REG, state);
    asm_emit_test_r64_r64(value_reg, value_reg, &state->writer);
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
