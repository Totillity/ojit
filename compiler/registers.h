#ifndef OJIT_REGISTERS_H
#define OJIT_REGISTERS_H

#include "compiler_records.h"
#include "emit_x64.h"

// region Registers
#define GET_LOC(value) ((value)->base.loc)

//void static inline mark_reg(enum Registers reg, struct AssemblerState* state);
//
//void static inline mark_loc(VLoc loc, struct AssemblerState* state);
//
//void static inline unmark_reg(enum Registers reg, struct AssemblerState* state);
//
//void static inline unmark_loc(VLoc loc, struct AssemblerState* state);
//
//bool static inline loc_is_marked(VLoc loc, struct AssemblerState* state);
//
//enum Registers get_unused(const bool* registers);
//
//enum Registers get_unused_tmp(const bool* registers);
//
//VLoc* instr_assign_loc(Instruction* instr, VLoc suggested, struct AssemblerState* state);
//
//VLoc* assign_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state);
//
//enum Registers postload_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state);
//
//enum Registers store_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state);
//
//void load_loc(VLoc* loc, struct AssemblerState* state);
//
//void prestore_loc(VLoc* loc, struct AssemblerState* state);
//
//void load_loc_into(VLoc* loc, enum Registers reg, struct AssemblerState* state);
//// endregion
//
//void static inline emit_assert_loc_i32(VLoc check_loc, struct AssemblerState* state);
//
//void static inline emit_assert_instr_i32(Instruction* instr, struct AssemblerState* state);
//
//void static inline emit_wrap_int_i32(VLoc* loc, uint32_t constant, struct AssemblerState* state);

void static inline mark_reg(enum Registers reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == false, "Attempted to mark a register which is already marked");
    state->used_registers[reg] = true;
}

void static inline mark_loc(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) mark_reg(loc.reg, state);
}

void static inline unmark_reg(enum Registers reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == true, "Attempted to unmark a register which is already unmarked");
    state->used_registers[reg] = false;
}

void static inline unmark_loc(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) unmark_reg(loc.reg, state);
}

bool static inline loc_is_marked(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) return state->used_registers[loc.reg];
    else return false;
}

enum Registers get_unused(const bool* registers) {
    if      (!registers[RAX]) return RAX;
    else if (!registers[RCX]) return RCX;
    else if (!registers[RDX]) return RDX;
    else if (!registers[RBX]) return RBX;
    else if (!registers[R8])  return R8;
    else if (!registers[R9])  return R9;
    else if (!registers[R10]) return R10;
    else if (!registers[R11]) return R11;
    else if (!registers[R14]) return R14;
    else if (!registers[R15]) return R15;
    else                      return NO_REG;
}

enum Registers get_unused_tmp(const bool* registers) {
    if      (!registers[R12]) return R12;
    else if (!registers[R13]) return R13;
    else                      return NO_REG;
}

VLoc* instr_assign_loc(Instruction* instr, VLoc suggested, struct AssemblerState* state) {
    VLoc* loc = &GET_LOC(instr);
    if (!IS_ASSIGNED(*loc)) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR && IS_ASSIGNED(instr->ir_parameter.entry_loc) && !loc_is_marked(instr->ir_parameter.entry_loc, state)) {
            *loc = instr->ir_parameter.entry_loc;
        } else if (IS_ASSIGNED(suggested) && !loc_is_marked(suggested, state) && suggested.reg != NO_REG) {
            // Since NO_REG will always be assigned, we don't need to have a specific check
            *loc = suggested;
        } else {
            enum Registers unused = get_unused(state->used_registers);
            if (unused == NO_REG) {
                uint8_t offset = state_alloc_var(state);
                *loc = WRAP_VAR(offset);
            } else {
                *loc = WRAP_REG(unused);
            }
        }
        mark_loc(*loc, state);
    }
    return loc;
}

VLoc* assign_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state) {
    if (!IS_ASSIGNED(*loc)) {
        // Since NO_REG will always be assigned, we don't need to have a specific check
        if (IS_ASSIGNED(suggested) && !loc_is_marked(suggested, state) && suggested.reg != NO_REG) {
            *loc = suggested;
        } else {
            enum Registers unused = get_unused(state->used_registers);
            if (unused == NO_REG) {
                uint8_t offset = state_alloc_var(state);
                *loc = WRAP_VAR(offset);
            } else {
                *loc = WRAP_REG(unused);
            }
        }
        mark_loc(*loc, state);
    }
    return loc;
}

enum Registers postload_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state) {
    // Use these whenever we need something in a register, otherwise we can just use `assign_loc`
    assign_loc(loc, suggested, state);
    if (!loc->is_reg) {
        loc->reg = get_unused_tmp(state->used_registers);
        mark_reg(loc->reg, state);
    }
    return loc->reg;
}

enum Registers store_loc(VLoc* loc, VLoc suggested, struct AssemblerState* state) {
    // Use these whenever we need something in a register, otherwise we can just use `assign_loc`
    assign_loc(loc, suggested, state);
    if (!loc->is_reg) {
        loc->reg = get_unused_tmp(state->used_registers);
        asm_emit_store_with_offset(RBP, loc->offset * 8, loc->reg, &state->writer);
        mark_reg(loc->reg, state);
    }
    return loc->reg;
}

void load_loc(VLoc* loc, struct AssemblerState* state) {
    if (!loc->is_reg) {
        asm_emit_load_with_offset(loc->reg, RBP, loc->offset * 8, &state->writer);
        unmark_reg(loc->reg, state);
        loc->reg = SPILLED_REG;
    }
}

void prestore_loc(VLoc* loc, struct AssemblerState* state) {
    if (!loc->is_reg) {
        unmark_reg(loc->reg, state);
        loc->reg = SPILLED_REG;
    }
}

void load_loc_into(VLoc* loc, enum Registers reg, struct AssemblerState* state) {
    if (!loc->is_reg) {
        asm_emit_load_with_offset(reg, RBP, loc->offset * 8, &state->writer);
    } else {
        asm_emit_mov_r64_r64(reg, loc->reg, &state->writer);
    }
}
// endregion

void static inline emit_assert_loc_i32(VLoc check_loc, struct AssemblerState* state) {
    Segment* this_err_label = state->errs_label;
    struct AssemblyWriter old_writer = state->writer;
    state->writer.label = this_err_label;
    Segment* err_segment = state->writer.curr = create_segment_code(this_err_label, state->err_return_label, state->writer.write_mem);
    asm_emit_jmp(state->err_return_label, &state->writer);
    asm_emit_mov_r64_i64(RCX, INT_AS_VAL(1), &state->writer);
    state->writer = old_writer;

    enum Registers tmp_reg = get_unused_tmp(state->used_registers);
    asm_emit_jcc(IF_NOT_EQUAL, this_err_label, &state->writer);
    asm_emit_cmp_r64_i32(tmp_reg, 0b001, &state->writer);
    asm_emit_shr_r64_i8(tmp_reg, 48, &state->writer);
    asm_emit_mov(WRAP_REG(tmp_reg), check_loc, &state->writer);
    state->errs_label = create_segment_label(err_segment, state->err_return_label, state->writer.write_mem);
}

void static inline emit_assert_instr_i32(Instruction* instr, struct AssemblerState* state) {
    if (instr->base.type == TYPE_INT)
        return;
    emit_assert_loc_i32(GET_LOC(instr), state);
}

void static inline emit_wrap_int_i32(VLoc* loc, uint32_t constant, struct AssemblerState* state) {
    uint64_t value = 0;
    value += 0b001ull << 48;
    value += constant;
    if (loc->is_reg) {
        asm_emit_mov_r64_i64(loc->reg, value, &state->writer);
    } else {
        asm_emit_mov(*loc, WRAP_REG(TMP_1_REG), &state->writer);
        asm_emit_mov_r64_i64(TMP_1_REG, value, &state->writer);
    }
}

#endif //OJIT_REGISTERS_H
