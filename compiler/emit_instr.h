#ifndef OJIT_EMIT_INSTR_H
#define OJIT_EMIT_INSTR_H

#include "../asm_ir.h"
#include "registers.h"

// region Emit Instructions
void __attribute__((always_inline)) emit_int(Instruction* instruction, struct AssemblerState* state) {
    struct IntIR* instr = &instruction->ir_int;
    if (IS_ASSIGNED(GET_REG(instr))) {
//        asm_emit_mov_r64_i64(GET_REG(instr), instr->constant, state);
        emit_wrap_int_i32(GET_REG(instr), instr->constant, state);
        unmark_register(instr->base.reg, state);
    }
}

void __attribute__((always_inline)) emit_add(Instruction* instruction, struct AssemblerState* state) {
    struct AddIR* instr = &instruction->ir_add;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);

    Register64 a_register = GET_REG(instr->a);
    Register64 b_register = GET_REG(instr->b);
    bool a_assigned = IS_ASSIGNED(a_register);
    bool b_assigned = IS_ASSIGNED(b_register);

    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        Register64 add_to;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            add_to = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            add_to = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        asm_emit_add_r64_i32(this_reg, constant, &state->writer);
        asm_emit_mov_r64_r64(this_reg, add_to, &state->writer);
        emit_assert_int_i32(add_to, state);
        return;
    }
#endif

    if (a_assigned && b_assigned) {
        // we need to copy a into primary_reg, then add b into it
        asm_emit_add_r64_r64(this_reg, b_register, &state->writer);
        asm_emit_mov_r64_r64(this_reg, a_register, &state->writer);
        goto emit_check;
    } else {
        Register64 primary_reg;  // the register we add into to get the result
        Register64 secondary_reg; // the register we add in

        if (a_assigned) {
            // use b as our primary register which is added into and contains the result
            // after all, this is b's last (or first) use, so it's safe
            instr_assign_reg(instr->b, this_reg);
            mark_register(this_reg, state);
            primary_reg = this_reg;
            secondary_reg = a_register;
        } else if (b_assigned) {
            // use a as our primary register which is added into and contains the result
            // after all, this is a's last (or first) use, so it's safe
            instr_assign_reg(instr->a, this_reg);
            mark_register(this_reg, state);
            primary_reg = this_reg;
            secondary_reg = b_register;
        } else {
            a_register = instr_fetch_reg(instr->a, this_reg, state);
            b_register = instr_fetch_reg(instr->b, this_reg, state);
            if (a_register == this_reg || b_register == this_reg) {
                if (a_register == this_reg) {
                    secondary_reg = b_register;
                } else {
                    secondary_reg = a_register;
                }
                asm_emit_add_r64_r64(this_reg, secondary_reg, &state->writer);
            } else {
                asm_emit_add_r64_r64(this_reg, b_register, &state->writer);
                asm_emit_mov_r64_r64(this_reg, a_register, &state->writer);
            }
            goto emit_check;
        }
        asm_emit_add_r64_r64(primary_reg, secondary_reg, &state->writer);
        goto emit_check;
    }
    emit_check:
    emit_assert_int_i32(b_register, state);
    emit_assert_int_i32(a_register, state);
}

void __attribute__((always_inline)) emit_sub(Instruction* instruction, struct AssemblerState* state) {
    struct SubIR* instr = &instruction->ir_sub;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

    Register64 a_register = GET_REG(instr->a);
    Register64 b_register = GET_REG(instr->b);
    bool a_assigned = IS_ASSIGNED(a_register);
    bool b_assigned = IS_ASSIGNED(b_register);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        Register64 sub_from;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            sub_from = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            sub_from = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        asm_emit_sub_r64_i32(this_reg, constant, &state->writer);
        asm_emit_mov_r64_r64(this_reg, sub_from, &state->writer);
        return;
    }
#endif

    if (a_assigned && b_assigned) {
        // we need to copy a into primary_reg, then add b into it
        asm_emit_sub_r64_r64(this_reg, b_register, &state->writer);
        asm_emit_mov_r64_r64(this_reg, a_register, &state->writer);
    } else {
        Register64 primary_reg;  // the register we add into to get the result
        Register64 secondary_reg; // the register we add in

        if (a_assigned) {
            // use b as our primary register which is added into and contains the result
            // after all, this is b's last (or first) use, so it's safe
            instr_assign_reg(instr->b, this_reg);
            mark_register(this_reg, state);
            primary_reg = this_reg;
            secondary_reg = a_register;
        } else if (b_assigned) {
            // use a as our primary register which is added into and contains the result
            // after all, this is a's last (or first) use, so it's safe
            instr_assign_reg(instr->a, this_reg);
            mark_register(this_reg, state);
            primary_reg = this_reg;
            secondary_reg = b_register;
        } else {
            // use a as our primary register which is added into and contains the result
            // after all, this is a's last (or first) use, so it's safe
            instr_assign_reg(instr->a, this_reg);
            mark_register(this_reg, state);

            // now find a register to put b into
            Register64 new_reg = get_unused(state->used_registers);
            if (new_reg == NO_REG) {
                // TODO spill
                ojit_new_error();
                ojit_build_error_chars("Too many registers used concurrently");
                ojit_exit(-1);
                exit(-1);
            }
            instr_assign_reg(instr->b, new_reg);
            mark_register(new_reg, state);

            primary_reg = this_reg;
            secondary_reg = new_reg;
        }
        asm_emit_sub_r64_r64(primary_reg, secondary_reg, &state->writer);
    }
}

void __attribute__((always_inline)) emit_cmp(Instruction* instruction, struct AssemblerState* state, bool store) {
    struct CompareIR* instr = &instruction->ir_cmp;

    if (store && !IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 a_register = instr_fetch_reg(instr->a, NO_REG, state);
    Register64 b_register = instr_fetch_reg(instr->b, NO_REG, state);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        Register64 cmp_with;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            cmp_with = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            cmp_with = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        if (store) asm_emit_setcc(instr->cmp, this_reg, &state->writer);
        asm_emit_cmp_r32_i32(cmp_with, constant, &state->writer);
        emit_assert_int_i32(cmp_with, state);
        return;
    }
#endif
    if (store) asm_emit_setcc(instr->cmp, this_reg, &state->writer);
    asm_emit_cmp_r64_r64(a_register, b_register, &state->writer);
}

void __attribute__((always_inline)) emit_block_parameter(Instruction* instruction, struct AssemblerState* state) {
    struct ParameterIR* instr = &instruction->ir_parameter;

    Register64 this_reg = GET_REG(instr);
    if (!IS_ASSIGNED(this_reg)) return;
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

    Register64 entry_reg = instr->entry_reg;
    OJIT_ASSERT(entry_reg != NO_REG, "Error: Parameter entry register is unassigned");
#ifdef OJIT_OPTIMIZATIONS
#endif
    asm_emit_xchg_r64_r64(state->swap_owner_of[entry_reg], this_reg, &state->writer);
    Register64 original_this_content = state->swap_contents[this_reg];
    Register64 original_entry_owner = state->swap_owner_of[entry_reg];
    state->swap_contents[original_entry_owner] = original_this_content;
    state->swap_owner_of[original_this_content] = original_entry_owner;
    state->swap_owner_of[entry_reg] = this_reg;
    state->swap_contents[this_reg] = entry_reg;
}

void __attribute__((always_inline)) emit_global(Instruction* instruction, struct AssemblerState* state) {
    struct GlobalIR* instr = &instruction->ir_global;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov_r64_r64(this_reg, RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xc4, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_call_r64(RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xec, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_mov_r64_i64(RAX, (uint64_t) state->callback.compiled_callback, &state->writer);
    asm_emit_mov_r64_i64(RCX, (uint64_t) state->callback.jit_ptr, &state->writer);
    asm_emit_mov_r64_i64(RDX, (uint64_t) instr->name, &state->writer);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, &state->writer);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, &state->writer);
}

void __attribute__((always_inline)) emit_call(Instruction* instruction, struct AssemblerState* state) {
    struct CallIR* instr = &instruction->ir_call;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    // TODO here and above state saving
    bool push_rax = false;
    bool push_rdx = false;
    bool push_rcx = false;
    if (state->used_registers[RAX]) { asm_emit_pop_r64(RAX, &state->writer); push_rax = true;}
    if (state->used_registers[RDX]) { asm_emit_pop_r64(RDX, &state->writer); push_rdx = true;}
    if (state->used_registers[RCX]) { asm_emit_pop_r64(RCX, &state->writer); push_rcx = true;}

    enum Register64 callee_reg = instr->callee->base.reg;
    if (!IS_ASSIGNED(callee_reg)) {
        callee_reg = get_unused(state->used_registers);
        if (callee_reg == NO_REG) {
            // TODO spill
            ojit_new_error();
            ojit_build_error_chars("Too many registers used concurrently");
            ojit_exit(-1);
            exit(-1);
        }
        instr_assign_reg(instr->callee, callee_reg);
        mark_register(callee_reg, state);
    }

    asm_emit_mov_r64_r64(this_reg, RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xc4, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_call_r64(callee_reg, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xec, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);

    int arg_num = 0;
    FOREACH(arg_ptr, instr->arguments, IRValue) {
        IRValue arg = *arg_ptr;
        enum Register64 reg;
        switch (arg_num) {
            case 0: reg = RCX; break;
            case 1: reg = RDX; break;
            case 2: reg = R8; break;
            case 4: reg = R9; break;
            default: exit(-1);
        }
        asm_emit_mov_r64_r64(reg, arg->base.reg, &state->writer);
        arg_num++;
    }

    if (push_rcx) asm_emit_push_r64(RCX, &state->writer);
    if (push_rdx) asm_emit_push_r64(RDX, &state->writer);
    if (push_rax) asm_emit_push_r64(RAX, &state->writer);
}

void __attribute__((always_inline)) emit_get_attr(Instruction* instruction, struct AssemblerState* state) {
    struct GetAttrIR* instr = &instruction->ir_get_attr;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 obj_reg = instr_fetch_reg(instr->obj, RCX, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov_r64_r64(this_reg, RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xc4, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_call_r64(RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xec, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_mov_r64_i64(RAX, (uint64_t) hash_table_get_ptr, &state->writer);
    asm_emit_mov_r64_r64(RCX, obj_reg, &state->writer);
    asm_emit_mov_r64_i64(RDX, (uint64_t) instr->attr, &state->writer);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, &state->writer);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, &state->writer);
}

void __attribute__((always_inline)) emit_get_loc(Instruction* instruction, struct AssemblerState* state) {
    struct GetLocIR* instr = &instruction->ir_get_loc;
    OJIT_ASSERT(INSTR_TYPE(instr->loc) == ID_GET_ATTR_IR, "err");

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 loc_reg = instr_fetch_reg(instr->loc, this_reg, state);

    asm_emit_mov_r64_ir64(this_reg, loc_reg, &state->writer);
}

void __attribute__((always_inline)) emit_set_loc(Instruction* instruction, struct AssemblerState* state) {
    struct SetLocIR* instr = &instruction->ir_set_loc;
    OJIT_ASSERT(INSTR_TYPE(instr->loc) == ID_GET_ATTR_IR, "err");

    Register64 this_reg = GET_REG(instr);
    if (IS_ASSIGNED(this_reg)) {
        unmark_register(this_reg, state);
    }

    Register64 loc_reg = instr_fetch_reg(instr->loc, this_reg, state);
    Register64 value_reg = instr_fetch_reg(instr->value, this_reg, state);

    asm_emit_mov_ir64_r64(loc_reg, value_reg, &state->writer);
}

void __attribute__((always_inline)) emit_new_object(Instruction* instruction, struct AssemblerState* state) {
    struct NewObjectIR* instr = &instruction->ir_new_object;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov_r64_r64(this_reg, RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xc4, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_call_r64(RAX, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xec, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_mov_r64_i64(RAX, (uint64_t) new_hash_table, &state->writer);
    asm_emit_mov_r64_i64(RCX, (uint64_t) state->jit_mem, &state->writer);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, &state->writer);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, &state->writer);
}

void __attribute__((always_inline)) emit_instruction(Instruction* instruction_ir, struct AssemblerState* state) {
    switch (instruction_ir->base.id) {
        case ID_INT_IR: emit_int(instruction_ir, state); break;
        case ID_ADD_IR: emit_add(instruction_ir, state); break;
        case ID_SUB_IR: emit_sub(instruction_ir, state); break;
        case ID_CMP_IR: emit_cmp(instruction_ir, state, true); break;
        case ID_CALL_IR: emit_call(instruction_ir, state); break;
        case ID_GLOBAL_IR: emit_global(instruction_ir, state); break;
        case ID_GET_ATTR_IR: emit_get_attr(instruction_ir, state); break;
        case ID_GET_LOC_IR: emit_get_loc(instruction_ir, state); break;
        case ID_SET_LOC_IR: emit_set_loc(instruction_ir, state); break;
        case ID_NEW_OBJECT_IR: emit_new_object(instruction_ir, state); break;
        case ID_BLOCK_PARAMETER_IR: emit_block_parameter(instruction_ir, state); break;
        case ID_INSTR_NONE:
            ojit_new_error();
            ojit_build_error_chars("Broken or Unimplemented instruction: ");
            ojit_build_error_int(instruction_ir->base.id);
            ojit_error();
            exit(-1);
    }
}
// endregion

#endif //OJIT_EMIT_INSTR_H
