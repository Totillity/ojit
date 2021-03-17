#include <assert.h>
#include <stdio.h>

#include "ojit_def.h"
#include "compiler.h"
#ifdef OJIT_OPTIMIZATIONS
#include "ir_opt.h"
#endif


// region State and Records
struct OffsetRecord {
    uint32_t offset_from_end;
    struct BlockIR* target;
};

struct BlockRecord {
    struct BlockIR* block;
    size_t offset;
    LAList* last_mem_block;
    LAList* offset_records;
};

struct AssemblerState {
    struct BlockIR* block;
    size_t block_num;
    uint8_t* front_ptr;
    bool used_registers[16];
    uint32_t block_generated;
    LAList* newest_mem_block;
    LAList** offset_records_ptr;
    enum Register64 swap_owner_of[16];
    enum Register64 swap_contents[16];
    MemCtx* ctx;
    struct GetFunctionCallback callback;
};


void init_asm_state(struct AssemblerState* state, struct BlockIR* block, LAList* init_mem, LAList** offset_records_ptr, struct GetFunctionCallback callback) {
    state->block = block;
    state->block_num = block->block_num;
    state->front_ptr = &init_mem->mem[LALIST_BLOCK_SIZE];

    // Windows makes you assume the registers RBX, RSI, RDI, RBP, R12-R15 are used
    // Additionally, we assumed RBP, RSP, R12, and R13 are used because it's a pain to use them
    // 7 registers should be enough for anyone

    state->used_registers[RAX] = false;
    state->used_registers[RCX] = false;
    state->used_registers[RDX] = false;
    state->used_registers[RBX]         = true;
    state->used_registers[NO_REG]      = true;
    state->used_registers[SPILLED_REG] = true;
    state->used_registers[RSI]         = true;
    state->used_registers[RDI]         = true;
    state->used_registers[R8]  = false;
    state->used_registers[R9]  = false;
    state->used_registers[R10] = false;
    state->used_registers[R11] = false;
    state->used_registers[TMP_1_REG]   = true;
    state->used_registers[TMP_2_REG]   = true;
    state->used_registers[R14]         = true;
    state->used_registers[R15]         = true;

    for (int i = 0; i < 16; i++) {
        state->swap_owner_of[i] = i;
        state->swap_contents[i] = i;
    }

    state->block_generated = 0;
    state->offset_records_ptr = offset_records_ptr;
    state->newest_mem_block = init_mem;
    state->callback.compiled_callback = callback.compiled_callback;
    state->callback.jit_ptr = callback.jit_ptr;
}

uint32_t offset_from_end(struct AssemblerState* state) {
    uint32_t block_size = LALIST_BLOCK_SIZE - (state->front_ptr - state->newest_mem_block->mem);
    return block_size + state->block_generated;
}
// endregion

// region Registers
#define GET_REG(value) ((value)->base.reg)
#define SET_REG(value, reg_) ((value)->base.reg = (reg_))
#define REG_IS_MARKED(reg) (state->used_registers[(reg)])

void __attribute__((always_inline)) mark_register(Register64 reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == false, "Attempted to mark a register which is already marked");
//    assert(state->used_registers[reg] == false);
    state->used_registers[reg] = true;
}

void __attribute__((always_inline)) unmark_register(Register64 reg, struct AssemblerState* state) {
    OJIT_ASSERT(state->used_registers[reg] == true, "Attempted to unmark a register which is already unmarked");
//    assert(state->used_registers[reg] == true);
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
        if (state->used_registers[suggested]) {      // Since NO_REG will always be assigned, we don't need to have a specific check
            reg = get_unused(state->used_registers);
            if (reg == NO_REG) {
                printf("Too many registers used concurrently\n");
                exit(-1);  // TODO spilled registers
            }
        } else {
            reg = suggested;
        }
        instr_assign_reg(instr, reg);
        mark_register(reg, state);
    }
    return reg;
}
// endregion

// region Emit Assembly
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblerState* state) {
    if (state->front_ptr == state->newest_mem_block->mem) {
        state->newest_mem_block->len = LALIST_BLOCK_SIZE;   // TODO we can optimize this
        state->block_generated += state->newest_mem_block->len;
        state->newest_mem_block = lalist_grow(state->ctx, NULL, state->newest_mem_block);
        state->front_ptr = &state->newest_mem_block->mem[LALIST_BLOCK_SIZE];
    }
    *(--state->front_ptr) = byte;
}

void __attribute__((always_inline)) asm_emit_int8(uint8_t constant, struct AssemblerState* state) {
    asm_emit_byte(constant, state);
}

void __attribute__((always_inline)) asm_emit_int32(uint32_t constant, struct AssemblerState* state) {
    asm_emit_byte((constant >> 24) & 0xFF, state);
    asm_emit_byte((constant >> 16) & 0xFF, state);
    asm_emit_byte((constant >>  8) & 0xFF, state);
    asm_emit_byte((constant >>  0) & 0xFF, state);
}

void __attribute__((always_inline)) asm_emit_int64(uint64_t constant, struct AssemblerState* state) {
    asm_emit_byte((constant >> 56) & 0xFF, state);
    asm_emit_byte((constant >> 48) & 0xFF, state);
    asm_emit_byte((constant >> 40) & 0xFF, state);
    asm_emit_byte((constant >> 32) & 0xFF, state);
    asm_emit_byte((constant >> 24) & 0xFF, state);
    asm_emit_byte((constant >> 16) & 0xFF, state);
    asm_emit_byte((constant >>  8) & 0xFF, state);
    asm_emit_byte((constant >>  0) & 0xFF, state);
}

void __attribute__((always_inline)) asm_emit_test_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x85, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_xchg_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x87, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_mov_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x89, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_mov_r64_i64(Register64 dest, uint64_t constant, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
//    if (constant <= UINT8_MAX) {
//        asm_emit_int8(constant, state);
//        asm_emit_byte(0xB0 + (dest & 0b0111), state);
//        if (dest & 0b1000) {
//            asm_emit_byte(REX(0b0, 0b0, 0b0, dest >> 3 & 0b0001), state);
//        }
//        return;
//    }
    if (constant <= UINT32_MAX) {
        asm_emit_int32(constant, state);
        asm_emit_byte(0xB8 + (dest & 0b0111), state);
        if (dest & 0b1000) {
            asm_emit_byte(REX(0b0, 0b0, 0b0, dest >> 3 & 0b0001), state);
        }
        return;
    }
#endif
    asm_emit_int64(constant, state);
    asm_emit_byte(0xB8 + (dest & 0b0111), state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, dest >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_call_r64(Register64 reg, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, 0b10, reg & 0b0111), state);
    asm_emit_byte(0xFF, state);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), state);
    }
}

void __attribute__((always_inline)) asm_emit_pop_r64(Register64 reg, struct AssemblerState* state) {
    asm_emit_byte(0x58 + (reg & 0b0111), state);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), state);
    }
}

void __attribute__((always_inline)) asm_emit_push_r64(Register64 reg, struct AssemblerState* state) {
    asm_emit_byte(0x50 + (reg & 0b0111), state);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), state);
    }
}

void __attribute__((always_inline)) asm_emit_add_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), state);
    asm_emit_byte(0x01, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_add_r64_i32(Register64 source, uint32_t constant, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, state);
        asm_emit_byte(MODRM(0b11, 0, source & 0b0111), state);
        asm_emit_byte(0x83, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, state);
        asm_emit_byte(0x05, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), state);
        return;
    }
#endif
    asm_emit_int32(constant, state);
    asm_emit_byte(MODRM(0b11, 0, source & 0b0111), state);
    asm_emit_byte(0x81, state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_sub_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x29, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_sub_r64_i32(Register64 source, uint32_t constant, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, state);
        asm_emit_byte(MODRM(0b11, 5, source & 0b0111), state);
        asm_emit_byte(0x83, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, state);
        asm_emit_byte(0x2D, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), state);
        return;
    }
#endif
    asm_emit_int32(constant, state);
    asm_emit_byte(MODRM(0b11, 5, source & 0b0111), state);
    asm_emit_byte(0x81, state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_r64(Register64 a, Register64 b, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, b & 0b111, a & 0b0111), state);
    asm_emit_byte(0x39, state);
    asm_emit_byte(REX(0b1, b >> 3 & 0b1, 0b0, a >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_i32(Register64 source, uint32_t constant, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, state);
        asm_emit_byte(MODRM(0b11, 7, source & 0b0111), state);
        asm_emit_byte(0x83, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, state);
        asm_emit_byte(0x3D, state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), state);
        return;
    }
#endif
    asm_emit_int32(constant, state);
    asm_emit_byte(MODRM(0b11, 7, source & 0b0111), state);
    asm_emit_byte(0x81, state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_setcc(enum Comparison cond, enum Register64 reg, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, 0, reg & 0b0111), state);
    asm_emit_byte(cond + 0x10, state);
    asm_emit_byte(0x0F, state);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0, 1, 0, 0), state);
    }
}

void __attribute__((always_inline)) asm_emit_jmp(struct BlockIR* target, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (target->block_num - state->block_num == 1) return;
#endif
    struct OffsetRecord* record_ptr = lalist_grow_add(state->offset_records_ptr, sizeof(struct OffsetRecord));
    record_ptr->offset_from_end = offset_from_end(state);
    record_ptr->target = target;

    asm_emit_int32(0xEFBEADDE, state);
    asm_emit_byte(0xE9, state);
}

void __attribute__((always_inline)) asm_emit_jcc(enum Comparison cond, struct BlockIR* target, struct AssemblerState* state) {
    // TODO: If I subtract 0x10 from the cond-code, I can make the offsets 1 byte
#ifdef OJIT_OPTIMIZATIONS
    if (target->block_num - state->block_num == 1) return;
#endif
    struct OffsetRecord* record_ptr = lalist_grow_add(state->offset_records_ptr, sizeof(struct OffsetRecord));
    record_ptr->offset_from_end = offset_from_end(state);
    record_ptr->target = target;

    asm_emit_int32(0xEFBEADDE, state);
    asm_emit_byte(cond, state);
    asm_emit_byte(0x0F, state);
}
// endregion

// region Utility
#define TYPE_OF(val) ((val)->base.id)
// endregion

// region Emit
// region Emit Terminators
void __attribute__((always_inline)) emit_return(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct ReturnIR* ret = &terminator->ir_return;
    Register64 value_reg = instr_fetch_reg(ret->value, RAX, state);
    asm_emit_byte(0xC3, state);
    if (value_reg != RAX) {
        asm_emit_mov_r64_r64(RAX, value_reg, state);
    }
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
            if (param->var_name) {
                IRValue argument;
                hash_table_get(&state->block->variables, STRING_KEY(param->var_name), (uint64_t*) &argument);

                Register64 param_reg = param->entry_reg;
                Register64 argument_reg = GET_REG(argument);

                if (!IS_ASSIGNED(param_reg)) {
                    if (target_registers[argument_reg]) {
                        for (Register64 reg = 0; reg < 16; reg++) {
                            if (!target_registers[reg] && !state->used_registers[reg]) {
                                param_reg = reg;
                                break;
                            }
                        }
                        if (param_reg == NO_REG) {
                            printf("Too many registers used concurrently");
                            exit(-1);  // TODO register spilling
                        }
                    } else {
                        param_reg = argument_reg;
                    }
                    target_registers[param_reg] = true;
                    param->entry_reg = param_reg;
                } else {
                    if (REG_IS_MARKED(param_reg)) {
                        printf("something here also used the register the argument needs to go into\n");
                        exit(-1);  // TODO oh god not again
                    }
                }
                if (!IS_ASSIGNED(argument_reg)) {
                    argument_reg = instr_fetch_reg(argument, param_reg, state);
                }
                asm_emit_mov_r64_r64(param_reg, argument_reg, state);
            }
        } else {
            break;
        }
    }
}

void __attribute__((always_inline)) emit_branch(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct BranchIR* branch = &terminator->ir_branch;

    asm_emit_jmp(branch->target, state);  // comeback to this later after I've stitched everything together

    resolve_branch(branch->target, state);
}

void __attribute__((always_inline)) emit_cbranch(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct CBranchIR* cbranch = &terminator->ir_cbranch;

    asm_emit_jcc(IF_NOT_ZERO, cbranch->true_target, state);
    resolve_branch(cbranch->true_target, state);
    asm_emit_jcc(IF_ZERO, cbranch->false_target, state);
    resolve_branch(cbranch->false_target, state);

    Register64 value_reg = instr_fetch_reg(cbranch->cond, NO_REG, state);
    asm_emit_test_r64_r64(value_reg, value_reg, state);
}
// endregion

// region Emit Instructions
void __attribute__((always_inline)) emit_int(Instruction* instruction, struct AssemblerState* state) {
    struct IntIR* instr = &instruction->ir_int;
    if (IS_ASSIGNED(GET_REG(instr))) {
        asm_emit_mov_r64_i64(GET_REG(instr), instr->constant, state);
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
    if (TYPE_OF(instr->a) == ID_INT_IR || TYPE_OF(instr->b) == ID_INT_IR) {
        Register64 add_to;
        uint32_t constant;
        if (TYPE_OF(instr->a) == ID_INT_IR) {
            add_to = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            add_to = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        asm_emit_add_r64_i32(add_to, constant, state);
        asm_emit_mov_r64_r64(this_reg, add_to, state);
        return;
    }
#endif

    if (a_assigned && b_assigned) {
        // we need to copy a into primary_reg, then add b into it
        asm_emit_add_r64_r64(this_reg, b_register, state);
        asm_emit_mov_r64_r64(this_reg, a_register, state);
        return;
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
                printf("Too many registers used concurrently");
                exit(-1); // TODO spill
            }
            instr_assign_reg(instr->b, new_reg);
            mark_register(new_reg, state);

            primary_reg = this_reg;
            secondary_reg = new_reg;
        }
        asm_emit_add_r64_r64(primary_reg, secondary_reg, state);
        return;
    }
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
    if (TYPE_OF(instr->a) == ID_INT_IR || TYPE_OF(instr->b) == ID_INT_IR) {
        Register64 sub_from;
        uint32_t constant;
        if (TYPE_OF(instr->a) == ID_INT_IR) {
            sub_from = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            sub_from = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        asm_emit_sub_r64_i32(sub_from, constant, state);
        asm_emit_mov_r64_r64(this_reg, sub_from, state);
        return;
    }
#endif

    if (a_assigned && b_assigned) {
        // we need to copy a into primary_reg, then add b into it
        asm_emit_sub_r64_r64(this_reg, b_register, state);
        asm_emit_mov_r64_r64(this_reg, a_register, state);
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
                printf("Too many registers used concurrently");
                exit(-1); // TODO spill
            }
            instr_assign_reg(instr->b, new_reg);
            mark_register(new_reg, state);

            primary_reg = this_reg;
            secondary_reg = new_reg;
        }
        asm_emit_sub_r64_r64(primary_reg, secondary_reg, state);
    }
}

void __attribute__((always_inline)) emit_cmp(Instruction* instruction, struct AssemblerState* state) {
    struct CompareIR* instr = &instruction->ir_cmp;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 a_register = instr_fetch_reg(instr->a, NO_REG, state);
    Register64 b_register = instr_fetch_reg(instr->b, NO_REG, state);

#ifdef OJIT_OPTIMIZATIONS
    if (TYPE_OF(instr->a) == ID_INT_IR || TYPE_OF(instr->b) == ID_INT_IR) {
        Register64 cmp_with;
        uint32_t constant;
        if (TYPE_OF(instr->a) == ID_INT_IR) {
            cmp_with = instr_fetch_reg(instr->b, this_reg, state);
            constant = instr->a->ir_int.constant;
        } else {
            cmp_with = instr_fetch_reg(instr->a, this_reg, state);
            constant = instr->b->ir_int.constant;
        }
        asm_emit_setcc(instr->cmp, this_reg, state);
        asm_emit_cmp_r64_i32(cmp_with, constant, state);
        return;
    }
#endif
    asm_emit_setcc(instr->cmp, this_reg, state);
    asm_emit_cmp_r64_r64(a_register, b_register, state);
}


void __attribute__((always_inline)) emit_block_parameter(Instruction* instruction, struct AssemblerState* state) {
    struct ParameterIR* instr = &instruction->ir_parameter;

    Register64 this_reg = GET_REG(instr);
    if (!IS_ASSIGNED(this_reg)) return;
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

    Register64 entry_reg = instr->entry_reg;
    asm_emit_xchg_r64_r64(state->swap_owner_of[entry_reg], this_reg, state);
    state->swap_owner_of[entry_reg] = this_reg;
}

void __attribute__((always_inline)) emit_global(Instruction* instruction, struct AssemblerState* state) {
    struct GlobalIR* instr = &instruction->ir_global;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    if (state->used_registers[RAX]) asm_emit_pop_r64(RAX, state);
    if (state->used_registers[RDX]) asm_emit_pop_r64(RDX, state);
    if (state->used_registers[RCX]) asm_emit_pop_r64(RCX, state);

    asm_emit_mov_r64_r64(this_reg, RAX, state);
    asm_emit_byte(0x20, state);
    asm_emit_byte(0xc4, state);
    asm_emit_byte(0x83, state);
    asm_emit_byte(0x48, state);
    asm_emit_call_r64(RAX, state);
    asm_emit_byte(0x20, state);
    asm_emit_byte(0xec, state);
    asm_emit_byte(0x83, state);
    asm_emit_byte(0x48, state);
    asm_emit_mov_r64_i64(RAX, (uint64_t) state->callback.compiled_callback, state);
    asm_emit_mov_r64_i64(RCX, (uint64_t) state->callback.jit_ptr, state);
    asm_emit_mov_r64_i64(RDX, (uint64_t) instr->name, state);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, state);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, state);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, state);
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
    if (state->used_registers[RAX]) { asm_emit_pop_r64(RAX, state); push_rax = true;}
    if (state->used_registers[RDX]) { asm_emit_pop_r64(RDX, state); push_rdx = true;}
    if (state->used_registers[RCX]) { asm_emit_pop_r64(RCX, state); push_rcx = true;}

    enum Register64 callee_reg = instr->callee->base.reg;
    if (!IS_ASSIGNED(callee_reg)) {
        callee_reg = get_unused(state->used_registers);
        if (callee_reg == NO_REG) {
            printf("Too many registers used concurrently");
            exit(-1); // TODO spill
        }
        instr_assign_reg(instr->callee, callee_reg);
        mark_register(callee_reg, state);
    }

    asm_emit_mov_r64_r64(this_reg, RAX, state);
    asm_emit_byte(0x20, state);
    asm_emit_byte(0xc4, state);
    asm_emit_byte(0x83, state);
    asm_emit_byte(0x48, state);
    asm_emit_call_r64(callee_reg, state);
    asm_emit_byte(0x20, state);
    asm_emit_byte(0xec, state);
    asm_emit_byte(0x83, state);
    asm_emit_byte(0x48, state);

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
        asm_emit_mov_r64_r64(reg, arg->base.reg, state);
        arg_num++;
    }

    if (push_rcx) asm_emit_push_r64(RCX, state);
    if (push_rdx) asm_emit_push_r64(RDX, state);
    if (push_rax) asm_emit_push_r64(RAX, state);
}
// endregion

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

void __attribute__((always_inline)) emit_instruction(Instruction* instruction_ir, struct AssemblerState* state) {
    switch (instruction_ir->base.id) {
        case ID_INT_IR: emit_int(instruction_ir, state); break;
        case ID_ADD_IR: emit_add(instruction_ir, state); break;
        case ID_SUB_IR: emit_sub(instruction_ir, state); break;
        case ID_CMP_IR: emit_cmp(instruction_ir, state); break;
        case ID_CALL_IR: emit_call(instruction_ir, state); break;
        case ID_GLOBAL_IR: emit_global(instruction_ir, state); break;
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

// region Debug
int get_var_num(IRValue var, struct HashTable* table) {
    uint64_t ret;
    if (hash_table_get(table, HASH_KEY(var), &ret)) {
        return ret;
    } else {
        ret = table->len;
        hash_table_insert(table, HASH_KEY(var), ret);
        return ret;
    }
}


void dump_function(struct FunctionIR* func) {
    printf("FUNCTION ");
    int c_i = 0;
    while (c_i < func->name->length) {
        putchar(func->name->start_ptr[c_i]);
        c_i += 1;
    }
    printf("\n");

    struct HashTable var_names;
    MemCtx* tmp_mem = create_mem_ctx();
    init_hash_table(&var_names, tmp_mem);

    LAListIter block_iter;
    lalist_init_iter(&block_iter, func->first_blocks, sizeof(struct BlockIR));
    struct BlockIR* block = lalist_iter_next(&block_iter);
    int k = 0;

    while (k < func->num_blocks) {
        printf("    BLOCK @%zu\n", block->block_num);

        FOREACH_INSTR(instr, block->first_instrs) {
            int i = get_var_num(instr, &var_names);
#ifdef OJIT_READABLE_IR
            if (instr->base.is_disabled) {
                printf("        (LIKELY DISABLED) ");
            } else {
                printf("        ");
            }
#else
            printf("        ");
#endif
            switch (instr->base.id) {
                case ID_INT_IR: {
                    printf("$%i = INT32 %d\n", i, instr->ir_int.constant);
                    break;
                }
                case ID_BLOCK_PARAMETER_IR: {
                    if (instr->ir_parameter.var_name) {
                        printf("$%i = PARAMETER \"", i);

                        int c_ = 0;
                        while (c_ < instr->ir_parameter.var_name->length) {
                            putchar(instr->ir_parameter.var_name->start_ptr[c_]);
                            c_ += 1;
                        }
                        printf("\"\n");
                    } else {
                        printf("$%i = PARAMETER (DISABLED)\n", i);
                    }
                    break;
                }
                case ID_ADD_IR: {
                    printf("$%i = ADD $%i, $%i\n", i, get_var_num(instr->ir_add.a, &var_names), get_var_num(instr->ir_add.b, &var_names));
                    break;
                }
                case ID_SUB_IR: {
                    printf("$%i = SUB $%i, $%i\n", i, get_var_num(instr->ir_sub.a, &var_names), get_var_num(instr->ir_sub.b, &var_names));
                    break;
                }
                case ID_CMP_IR: {
                    printf("$%i = CMP (%i) $%i, $%i\n", i, instr->ir_cmp.cmp, get_var_num(instr->ir_cmp.a, &var_names), get_var_num(instr->ir_cmp.b, &var_names));
                    break;
                }
                case ID_CALL_IR: {
                    printf("$%i = CALL\n", i);
                    break;
                }
                case ID_GLOBAL_IR: {
                    printf("$%i = GLOBAL\n", i);
                    break;
                }
                case ID_INSTR_NONE: {
                    printf("$%i = UNKNOWN\n", i);
                    break;
                }
            }
        }
        switch (block->terminator.ir_base.id) {
            case ID_BRANCH_IR: {
                printf("        BRANCH @%zu\n", block->terminator.ir_branch.target->block_num);
                break;
            }
            case ID_CBRANCH_IR: {
                printf("        CBRANCH $%i (true: @%zu, false: @%zu)\n",
                       get_var_num(block->terminator.ir_cbranch.cond, &var_names),
                       block->terminator.ir_cbranch.true_target->block_num,
                       block->terminator.ir_cbranch.false_target->block_num);
                break;
            }
            case ID_RETURN_IR: {
                printf("        RETURN $%i\n", get_var_num(block->terminator.ir_return.value, &var_names));
                break;
            }
            default: {
                printf("        UNKNOWN\n");
            }
        }

        block = lalist_iter_next(&block_iter);
        k++;
    }
    destroy_mem_ctx(tmp_mem);
}
// endregion

// region Compile
void assign_function_parameters(struct FunctionIR* func) {
    struct BlockIR* first_block = lalist_get(func->first_blocks, sizeof(struct BlockIR), 0);
    int param_num = 0;
    FOREACH_INSTR(instr, first_block->first_instrs) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR) {
            enum Register64 reg;
            switch (param_num) {
                case 0: reg = RCX; break;
                case 1: reg = RDX; break;
                case 2: reg = R8; break;
                case 3: reg = R9; break;
                default: exit(-1);  // TODO
            }
            instr->ir_parameter.entry_reg = reg;
            param_num += 1;
        }
    }
}


struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback) {
#ifdef OJIT_OPTIMIZATIONS
    ojit_optimize_func(func, callback);
#endif
//    dump_function(func);

    struct AssemblerState state;
    state.ctx = compiler_mem;

    size_t generated_size = 0;
    struct BlockRecord* block_records = ojit_alloc(compiler_mem, sizeof(struct BlockRecord) * func->num_blocks);

    assign_function_parameters(func);

    int i = 0;
    FOREACH(block, func->first_blocks, struct BlockIR) {
        LAList* init_mem = lalist_new(compiler_mem);
        block_records[i].block = block;
        block_records[i].offset = generated_size;
        block_records[i].last_mem_block = init_mem;
        block_records[i].offset_records = lalist_new(compiler_mem);
        init_asm_state(&state, block, init_mem, &block_records[i].offset_records, callback);

        emit_terminator(&block->terminator, &state);

        LAListIter instr_iter;
        lalist_init_iter(&instr_iter, block->last_instrs, sizeof(Instruction));
        lalist_iter_position(&instr_iter, block->last_instrs->len);
        Instruction* instr = lalist_iter_prev(&instr_iter);
        int k = 0;
        while (instr) {
            emit_instruction(instr, &state);
            instr = lalist_iter_prev(&instr_iter);
            k += 1;
        }
        state.newest_mem_block->len = LALIST_BLOCK_SIZE - (state.front_ptr - state.newest_mem_block->mem);
        generated_size += offset_from_end(&state);

        i++;
    }

    uint8_t* func_mem = ojit_alloc(compiler_mem, generated_size);
    uint8_t* write_ptr = func_mem + generated_size;

    for (int b_i = func->num_blocks - 1; b_i >= 0; b_i--) {
        struct BlockRecord record = block_records[b_i];
        block = record.block;
        LAList* curr_mem_block = record.last_mem_block;
        uint8_t* start_ptr = write_ptr;
        while (curr_mem_block) {
            size_t mem_block_size = curr_mem_block->len;
            write_ptr -= mem_block_size;
            ojit_memcpy(write_ptr, curr_mem_block->mem + (LALIST_BLOCK_SIZE - mem_block_size), mem_block_size);

            curr_mem_block = curr_mem_block->prev;
        }

        int m = 0;
        FOREACH(offset_record, record.offset_records, struct OffsetRecord) {
            uint8_t* offset_ptr = (start_ptr - offset_record->offset_from_end);
            struct BlockRecord target_record = block_records[offset_record->target->block_num];
            uintptr_t offset = (func_mem + target_record.offset) - offset_ptr;
            *(offset_ptr - 4) = (uint8_t) (offset >> 0) & 0xFF;
            *(offset_ptr - 3) = (uint8_t) (offset >> 8) & 0xFF;
            *(offset_ptr - 2) = (uint8_t) (offset >> 16) & 0xFF;
            *(offset_ptr - 1) = (uint8_t) (offset >> 24) & 0xFF;
//            printf("Wrote %llu for %i at %p\n", offset, b_i, offset_ptr);
            m++;
        }
    }

    return (struct CompiledFunction) {func_mem, generated_size};
}

#ifdef WIN32
#include <windows.h>
void* copy_to_executable(void* from, size_t len) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    void* mem = VirtualAlloc(NULL, info.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    if (mem == NULL) {
        ojit_new_error();
        ojit_build_error_chars("Failed to move generated code to executable memory.\n");
        ojit_error();
        exit(0);
    }
    memcpy(mem, from, len);

    DWORD dummy;
    bool success = VirtualProtect(mem, len, PAGE_EXECUTE_READ, &dummy);
    if (!success) {
        ojit_new_error();
        ojit_build_error_chars("Failed to move generated code to executable memory.\n");
        ojit_error();
        exit(0);
    }

    return mem;
}
#else
#warning Executing jited code is currently unsupported on platforms other than Windows.
void* copy_to_executable(CState* bstate, void* from, size_t len) {
    ojit_fatal_error(bstate->error, "Executing jit'ed code is currently unsupported on platforms other than Windows.\n");
}
#endif
// endregion
