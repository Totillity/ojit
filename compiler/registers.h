#ifndef OJIT_REGISTERS_H
#define OJIT_REGISTERS_H

#include "compiler_records.h"
#include "emit_x64.h"

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

void __attribute__((always_inline)) emit_wrap_int_i32(Register64 to_reg, uint32_t constant, struct AssemblerState* state) {
    uint64_t value = 0;
    value += 0b001ull << 48;
    value += constant;
    asm_emit_mov_r64_i64(to_reg, value, &state->writer);
}

void __attribute__((always_inline)) emit_assert_int_i32(Register64 check_reg, struct AssemblerState* state) {
    Segment* this_err_label = state->errs_label;
    struct AssemblyWriter old_writer = state->writer;
    state->writer.label = this_err_label;
    Segment* err_segment = state->writer.curr = create_segment_code(this_err_label, state->err_return_label, state->writer.write_mem);
    asm_emit_jmp(state->err_return_label, &state->writer);
//    asm_emit_mov_r64_i64(RCX, ((uint64_t) this_err_label) & 0xFF, &state->writer);
    asm_emit_mov_r64_r64(RCX, RCX, &state->writer);
    state->writer = old_writer;

    asm_emit_jcc(IF_NOT_EQUAL, this_err_label, &state->writer);
    asm_emit_cmp_r64_i32(TMP_1_REG, 0b001, &state->writer);
    asm_emit_shr_r64_i8(TMP_1_REG, 48, &state->writer);
    asm_emit_mov_r64_r64(TMP_1_REG, check_reg, &state->writer);
    state->errs_label = create_segment_label(err_segment, state->err_return_label, state->writer.write_mem);
}

// region Registers
#define GET_REG(value) ((value)->base.reg)
#define SET_REG(value, reg_) ((value)->base.reg = (reg_))
#define REG_IS_MARKED(reg) (state->used_registers[(reg)])

void __attribute__((always_inline)) mark_register(Register64 reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == false, "Attempted to mark a register which is already marked");
    state->used_registers[reg] = true;
}

void __attribute__((always_inline)) unmark_register(Register64 reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == true, "Attempted to unmark a register which is already unmarked");
    state->used_registers[reg] = false;
}

void __attribute__((always_inline)) instr_assign_reg(Instruction* instr, Register64 reg) {
    OJIT_ASSERT(!IS_ASSIGNED(GET_REG(instr)), "Attempted to assign a register which is already in use");
    SET_REG(instr, reg);
}

Register64 get_unused(const bool* registers) {
    for (int i = 0; i < 16; i++) {
        if (!registers[i]) return i;
    }
    return NO_REG;
}

Register64 instr_fetch_reg(IRValue instr, Register64 suggested, struct AssemblerState* state) {
    Register64 reg = GET_REG(instr);
    if (!IS_ASSIGNED(reg)) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR && instr->ir_parameter.entry_reg != NO_REG && !state->used_registers[instr->ir_parameter.entry_reg]) {
            reg = instr->ir_parameter.entry_reg;
        } else if (!state->used_registers[suggested]) {      // Since NO_REG will always be assigned, we don't need to have a specific check
            reg = suggested;
        } else {
            reg = get_unused(state->used_registers);
            if (reg == NO_REG) {
                // TODO spill
                ojit_new_error();
                ojit_build_error_chars("Too many registers used concurrently");
                ojit_exit(-1);
                exit(-1);
            }
        }
        instr_assign_reg(instr, reg);
        mark_register(reg, state);
    }
    return reg;
}
// endregion

#endif //OJIT_REGISTERS_H
