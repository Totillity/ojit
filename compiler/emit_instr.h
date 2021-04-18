#ifndef OJIT_EMIT_INSTR_H
#define OJIT_EMIT_INSTR_H

#include "../asm_ir.h"
#include "registers.h"

// region Emit Instructions
void static inline emit_int(Instruction* instruction, struct AssemblerState* state) {
    struct IntIR* instr = &instruction->ir_int;
    if (IS_ASSIGNED(GET_LOC(instr))) {
        emit_wrap_int_i32(&GET_LOC(instr), instr->constant, state);
        unmark_loc(GET_LOC(instr), state);
    }
}

void static inline emit_add(Instruction* instruction, struct AssemblerState* state) {
    struct AddIR* instr = &instruction->ir_add;
    struct AssemblyWriter* writer = &state->writer;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_loc(this_loc, state);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        VLoc add_to;
        Instruction* check_instr;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            add_to = *instr_assign_loc(instr->b, this_loc, state);
            check_instr = instr->b;
            constant = instr->a->ir_int.constant;
        } else {
            add_to = *instr_assign_loc(instr->a, this_loc, state);
            check_instr = instr->a;
            constant = instr->b->ir_int.constant;
        }
        enum Registers tmp_reg = store_loc(&this_loc, WRAP_NONE(), state);
        asm_emit_add_r64_i32(tmp_reg, constant, &state->writer);
        load_loc(&this_loc, state);
        asm_emit_mov(this_loc, add_to, writer);
        emit_assert_instr_i32(check_instr, state);
        return;
    }
#endif
    VLoc a_loc = *instr_assign_loc(instr->a, this_loc, state);
    VLoc b_loc = *instr_assign_loc(instr->b, this_loc, state);

    if (loc_equal(a_loc, this_loc)) {
        asm_emit_add(this_loc, WRAP_REG(TMP_1_REG), writer);
        asm_emit_mov32(WRAP_REG(TMP_1_REG), b_loc, writer);
    } else if (loc_equal(b_loc, this_loc)) {
        asm_emit_add(this_loc, WRAP_REG(TMP_1_REG), writer);
        asm_emit_mov32(WRAP_REG(TMP_1_REG), a_loc, writer);
    } else {
        asm_emit_add(this_loc, b_loc, writer);
        asm_emit_mov32(this_loc, a_loc, writer);
    }

    emit_assert_instr_i32(instr->a, state);
    emit_assert_instr_i32(instr->b, state);
}

void static inline emit_sub(Instruction* instruction, struct AssemblerState* state) {
    struct SubIR* instr = &instruction->ir_sub;
    struct AssemblyWriter* writer = &state->writer;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_loc(this_loc, state);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        VLoc* add_to;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            add_to = instr_assign_loc(instr->b, this_loc, state);
            constant = instr->a->ir_int.constant;
        } else {
            add_to = instr_assign_loc(instr->a, this_loc, state);
            constant = instr->b->ir_int.constant;
        }
        enum Registers tmp_reg = store_loc(&this_loc, WRAP_NONE(), state);
        asm_emit_sub_r64_i32(tmp_reg, constant, &state->writer);
        load_loc(&this_loc, state);
        asm_emit_mov(this_loc, *add_to, writer);
        emit_assert_loc_i32(*add_to, state);
        return;
    }
#endif
    VLoc a_loc = *instr_assign_loc(instr->a, this_loc, state);
    VLoc b_loc = *instr_assign_loc(instr->b, this_loc, state);

    if (loc_equal(a_loc, this_loc)) {
        asm_emit_sub(this_loc, b_loc, writer);
    } else if (loc_equal(b_loc, this_loc)) {
        asm_emit_sub(this_loc, a_loc, writer);
    } else {
        asm_emit_sub(this_loc, b_loc, writer);
        asm_emit_mov(this_loc, a_loc, writer);
    }

    emit_assert_loc_i32(a_loc, state);
    emit_assert_loc_i32(b_loc, state);
}

void static inline emit_cmp(Instruction* instruction, struct AssemblerState* state, bool store) {
    struct CompareIR* instr = &instruction->ir_cmp;

    if (store && !IS_ASSIGNED(GET_LOC(instr))) return;
    if (store) ojit_exit(-1);   // TODO
    VLoc this_loc = GET_LOC(instr);
    if (IS_ASSIGNED(this_loc)) unmark_loc(this_loc, state);

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(instr->a) == ID_INT_IR || INSTR_TYPE(instr->b) == ID_INT_IR) {
        VLoc* cmp_with;
        Instruction* check_instr;
        uint32_t constant;
        if (INSTR_TYPE(instr->a) == ID_INT_IR) {
            cmp_with = instr_assign_loc(instr->b, this_loc, state);
            check_instr = instr->b;
            constant = instr->a->ir_int.constant;
        } else {
            cmp_with = instr_assign_loc(instr->a, this_loc, state);
            check_instr = instr->a;
            constant = instr->b->ir_int.constant;
        }
//        if (store) asm_emit_setcc(instr->cmp, this_loc, &state->writer);
        enum Registers reg = postload_loc(cmp_with, this_loc, state);
        asm_emit_cmp_r32_i32(reg, constant, &state->writer);
        if (check_instr->base.type != TYPE_INT)
            emit_assert_loc_i32(WRAP_REG(reg), state);
        load_loc(cmp_with, state);
        return;
    }
#endif

    VLoc* a_loc = instr_assign_loc(instr->a, WRAP_NONE(), state);
    VLoc* b_loc = instr_assign_loc(instr->b, WRAP_NONE(), state);

//    if (store) asm_emit_setcc(instr->cmp, this_loc, &state->writer);
    asm_emit_cmp(*a_loc, *b_loc, &state->writer);
}

void static inline emit_global(Instruction* instruction, struct AssemblerState* state) {
    struct GlobalIR* instr = &instruction->ir_global;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    unmark_loc(this_loc, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov(this_loc, WRAP_REG(RAX), &state->writer);
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

void static inline emit_call(Instruction* instruction, struct AssemblerState* state) {
    struct CallIR* instr = &instruction->ir_call;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    unmark_loc(this_loc, state);

    // TODO here and above state saving
    bool push_rax = false;
    bool push_rdx = false;
    bool push_rcx = false;
    if (state->used_registers[RAX]) { asm_emit_pop_r64(RAX, &state->writer); push_rax = true;}
    if (state->used_registers[RDX]) { asm_emit_pop_r64(RDX, &state->writer); push_rdx = true;}
    if (state->used_registers[RCX]) { asm_emit_pop_r64(RCX, &state->writer); push_rcx = true;}

    VLoc* callee_reg = instr_assign_loc(instr->callee, WRAP_REG(RAX), state);

    asm_emit_mov(this_loc, WRAP_REG(RAX), &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xc4, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);
    asm_emit_call(*callee_reg, &state->writer);
    asm_emit_byte(0x20, &state->writer);
    asm_emit_byte(0xec, &state->writer);
    asm_emit_byte(0x83, &state->writer);
    asm_emit_byte(0x48, &state->writer);

    int arg_num = 0;
    FOREACH(arg_ptr, instr->arguments, IRValue) {
        IRValue arg = *arg_ptr;
        VLoc reg;
        switch (arg_num) {
            case 0: reg = WRAP_REG(RCX); break;
            case 1: reg = WRAP_REG(RDX); break;
            case 2: reg = WRAP_REG(R8); break;
            case 4: reg = WRAP_REG(R9); break;
            default: exit(-1);
        }
        asm_emit_mov(reg, GET_LOC(arg), &state->writer);
        // TODO convert to swap
        arg_num++;
    }

    if (push_rcx) asm_emit_push_r64(RCX, &state->writer);
    if (push_rdx) asm_emit_push_r64(RDX, &state->writer);
    if (push_rax) asm_emit_push_r64(RAX, &state->writer);
}

void static inline emit_get_attr(Instruction* instruction, struct AssemblerState* state) {
    struct GetAttrIR* instr = &instruction->ir_get_attr;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    unmark_loc(this_loc, state);

    VLoc* obj_reg = instr_assign_loc(instr->obj, WRAP_REG(RCX), state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov(this_loc, WRAP_REG(RAX), &state->writer);
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
    asm_emit_mov(WRAP_REG(RCX), *obj_reg, &state->writer);
    asm_emit_mov_r64_i64(RDX, (uint64_t) instr->attr, &state->writer);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, &state->writer);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, &state->writer);
}

void static inline emit_get_loc(Instruction* instruction, struct AssemblerState* state) {
    struct GetLocIR* instr = &instruction->ir_get_loc;
    OJIT_ASSERT(INSTR_TYPE(instr->loc) == ID_GET_ATTR_IR, "err");

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    unmark_loc(this_loc, state);

    VLoc* loc_reg = instr_assign_loc(instr->loc, this_loc, state);

    asm_emit_mov(this_loc, *loc_reg, &state->writer);
}

void static inline emit_set_loc(Instruction* instruction, struct AssemblerState* state) {
    struct SetLocIR* instr = &instruction->ir_set_loc;
    OJIT_ASSERT(INSTR_TYPE(instr->loc) == ID_GET_ATTR_IR, "err");

    VLoc this_loc = GET_LOC(instr);
    if (IS_ASSIGNED(this_loc)) {
        unmark_loc(this_loc, state);
    }

    VLoc* loc_reg = instr_assign_loc(instr->loc, this_loc, state);
    VLoc* value_reg = instr_assign_loc(instr->value, this_loc, state);

    asm_emit_mov(*loc_reg, *value_reg, &state->writer);
}

void static inline emit_new_object(Instruction* instruction, struct AssemblerState* state) {
    struct NewObjectIR* instr = &instruction->ir_new_object;

    if (!IS_ASSIGNED(GET_LOC(instr))) return;
    VLoc this_loc = GET_LOC(instr);
    unmark_loc(this_loc, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, &state->writer);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, &state->writer);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, &state->writer);

    asm_emit_mov(this_loc, WRAP_REG(RAX), &state->writer);
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

void static inline emit_instruction(Instruction* instruction_ir, struct AssemblerState* state) {
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
        case ID_BLOCK_PARAMETER_IR: break;
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
