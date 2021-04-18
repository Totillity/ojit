#ifndef OJIT_REGISTERS_H
#define OJIT_REGISTERS_H

#include "compiler_records.h"
#include "emit_x64.h"

// region Registers
#define GET_LOC(value) ((value)->base.loc)
//#define SET_LOC(value, loc_) ((value)->base.loc = (loc_))

void __attribute__((always_inline)) mark_reg(enum Registers reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == false, "Attempted to mark a register which is already marked");
    state->used_registers[reg] = true;
}

void __attribute__((always_inline)) mark_loc(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) mark_reg(loc.reg, state);
}

void __attribute__((always_inline)) unmark_reg(enum Registers reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == true, "Attempted to unmark a register which is already unmarked");
    state->used_registers[reg] = false;
}

void __attribute__((always_inline)) unmark_loc(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) unmark_reg(loc.reg, state);
}

bool __attribute__((always_inline)) loc_is_marked(VLoc loc, struct AssemblerState* state) {
    if (loc.is_reg) return state->used_registers[loc.reg];
    else return false;
}

//void __attribute__((always_inline)) instr_assign_loc(Instruction* instr, VLoc loc) {
//    OJIT_ASSERT(!IS_ASSIGNED(GET_LOC(instr)), "Attempted to assign a register which is already in use");
//    SET_LOC(instr, loc);
//}

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

//Register64 instr_fetch_reg(IRValue instr, Register64 suggested, struct AssemblerState* state) {
//    Register64 reg = GET_REG(instr);
//    if (!IS_ASSIGNED(reg)) {
//        if (instr->base.id == ID_BLOCK_PARAMETER_IR && instr->ir_parameter.entry_loc != NO_REG && !state->used_registers[instr->ir_parameter.entry_loc]) {
//            reg = instr->ir_parameter.entry_loc;
//        } else if (!state->used_registers[suggested]) {      // Since NO_REG will always be assigned, we don't need to have a specific check
//            reg = suggested;
//        } else {
//            reg = get_unused(state->used_registers);
//            if (reg == NO_REG) {
//                // TODO spill
//                ojit_new_error();
//                ojit_build_error_chars("Too many registers used concurrently");
//                ojit_exit(-1);
//                exit(-1);
//            }
//        }
//        instr_assign_loc(instr, reg);
//        mark_loc(reg, state);
//    }
//    return reg;
//}

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

// ============ NAN Boxing ============
// Here are 64 bits:
//  0000000000000 000 000000000000000000000000000000000000000000000000
// |------A------|-B-|-----------------------C------------------------|
//   A: Check this. If its != 0, then its a double, otherwise its a boxed object.
//       Note: if it is a double, then you must invert the whole thing to restore it
//   B: Tag of the object (if it is one, otherwise its just double data)
//       Tags:
//           000: Pointer              (uses 48 bits)
//           001: Signed Integer     (uses 32 bits)
//           010: Undefined
//           100: Undefined
//   C: Payload. Size and usage vary based on type.


void __attribute__((always_inline)) emit_assert_int_i32(VLoc check_loc, struct AssemblerState* state) {
    Segment* this_err_label = state->errs_label;
    struct AssemblyWriter old_writer = state->writer;
    state->writer.label = this_err_label;
    Segment* err_segment = state->writer.curr = create_segment_code(this_err_label, state->err_return_label, state->writer.write_mem);
    asm_emit_jmp(state->err_return_label, &state->writer);
//    asm_emit_mov_r64_i64(RCX, ((uint64_t) this_err_label) & 0xFF, &state->writer);
    asm_emit_mov_r64_r64(RCX, RCX, &state->writer);
    state->writer = old_writer;

    enum Registers tmp_reg = get_unused_tmp(state->used_registers);
    asm_emit_jcc(IF_NOT_EQUAL, this_err_label, &state->writer);
    asm_emit_cmp_r64_i32(tmp_reg, 0b001, &state->writer);
    asm_emit_shr_r64_i8(tmp_reg, 48, &state->writer);
    asm_emit_mov(WRAP_REG(tmp_reg), check_loc, &state->writer);
    state->errs_label = create_segment_label(err_segment, state->err_return_label, state->writer.write_mem);
}

void __attribute__((always_inline)) emit_wrap_int_i32(VLoc* loc, uint32_t constant, struct AssemblerState* state) {
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
