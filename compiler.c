#include <assert.h>
#include <stdio.h>

#include "ojit_def.h"
#include "compiler.h"
#ifdef OJIT_OPTIMIZATIONS
#include "ir_opt.h"
#endif


// region State and Records
struct MemBlockBase {
    uint32_t len;
    bool is_code;
    union MemBlock* prev_block;
    union MemBlock* next_block;
};

struct MemBlockCode {
    struct MemBlockBase base;
    uint8_t code[512];
};

struct MemBlockJump {
    struct MemBlockBase base;
    uint8_t short_form[2];
    uint8_t long_form[6];
    struct BlockIR* target;
    bool is_short;
    uint32_t offset_from_end;
    struct MemBlockJump* next_jump;
    struct MemBlockJump* prev_jump;
};

union MemBlock {
    struct MemBlockBase base;
    struct MemBlockCode code;
    struct MemBlockJump jump;
};

struct BlockRecord {
    uint32_t max_offset_from_end;
    uint32_t actual_offset_from_end;
    union MemBlock* end_mem;
};

struct AssemblerState {
    struct BlockIR* block;

    union MemBlock* curr_mem;
    union MemBlock* end_mem;

    bool used_registers[16];
    uint32_t block_size;
    enum Register64 swap_owner_of[16];
    enum Register64 swap_contents[16];

    MemCtx* jit_mem;
    MemCtx* ctx;
    struct GetFunctionCallback callback;
};


void init_asm_state(struct AssemblerState* state, struct BlockIR* block, union MemBlock* end_mem, struct GetFunctionCallback callback) {
    state->block = block;
    state->curr_mem = state->end_mem = end_mem;

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

    state->block_size = 0;
    state->callback.compiled_callback = callback.compiled_callback;
    state->callback.jit_ptr = callback.jit_ptr;
}
// endregion

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
                printf("Too many registers used concurrently\n");
                exit(-1);  // TODO spilled registers
            }
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

union MemBlock* create_mem_block_code(union MemBlock* prev_block, union MemBlock* next_block, MemCtx* ctx) {
    struct MemBlockCode* code = &((union MemBlock*) ojit_alloc(ctx, sizeof(union MemBlock)))->code;
    code->base.len = 0;
    code->base.is_code = true;
    code->base.next_block = next_block;
    code->base.prev_block = prev_block;
    if (next_block) next_block->base.prev_block = (union MemBlock*) code;
    if (prev_block) prev_block->base.next_block = (union MemBlock*) code;
    return (union MemBlock*) code;
}

union MemBlock* create_mem_block_jump(union MemBlock* prev_block, union MemBlock* next_block, MemCtx* ctx) {
    struct MemBlockJump* jump = &((union MemBlock*) ojit_alloc(ctx, sizeof(union MemBlock)))->jump;
    jump->base.len = 0;
    jump->base.is_code = false;
    jump->base.next_block = next_block;
    jump->base.prev_block = prev_block;
    if (next_block) next_block->base.prev_block = (union MemBlock*) jump;
    if (prev_block) prev_block->base.next_block = (union MemBlock*) jump;
    return (union MemBlock*) jump;
}

void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblerState* state) {
    if (state->curr_mem->base.len >= 512) {
        state->curr_mem = create_mem_block_code(NULL, state->curr_mem, state->ctx);
    }
    state->curr_mem->base.len++;
    state->block_size++;
    state->curr_mem->code.code[512-state->curr_mem->base.len] = byte;
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

void __attribute__((always_inline)) asm_emit_xor_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), state);
    asm_emit_byte(0x33, state);
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

void __attribute__((always_inline)) asm_emit_mov_r64_ir64(Register64 dest, Register64 in_source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b00, dest & 0b111, in_source & 0b0111), state);
    asm_emit_byte(0x8B, state);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, in_source >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_mov_ir64_r64(Register64 in_dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b00, source & 0b111, in_dest & 0b0111), state);
    asm_emit_byte(0x89, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, in_dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_mov_r64_i64(Register64 dest, uint64_t constant, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant == 0) {
        asm_emit_xor_r64_r64(dest, dest, state);
        return;
    }
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
        asm_emit_byte(REX(0, 0, 0, 1), state);
    }
}

void __attribute__((always_inline)) asm_emit_jmp(struct BlockIR* target, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (target->prev_block == state->block) return;
#endif
    struct MemBlockJump* jump = (struct MemBlockJump*) create_mem_block_jump(NULL, state->curr_mem, state->ctx);
    jump->target = target;
    jump->base.len = 5;
    jump->long_form[0] = 0x00;
    jump->long_form[1] = 0xE9;
    jump->long_form[2] = 0xEF;
    jump->long_form[3] = 0xBE;
    jump->long_form[4] = 0xAD;
    jump->long_form[5] = 0xDE;

    jump->short_form[0] = 0xEB;
    jump->short_form[1] = 0xFF;
    jump->next_jump = NULL;
    jump->prev_jump = NULL;

    state->block_size += 5;

    state->curr_mem = create_mem_block_code(NULL, (union MemBlock*) jump, state->ctx);
}

void __attribute__((always_inline)) asm_emit_jcc(enum Comparison cond, struct BlockIR* target, struct AssemblerState* state) {
#ifdef OJIT_OPTIMIZATIONS
    if (target->prev_block == state->block) return;
#endif
    struct MemBlockJump* jump = (struct MemBlockJump*) create_mem_block_jump(NULL, state->curr_mem, state->ctx);
    jump->target = target;
    jump->base.len = 6;
    jump->long_form[0] = 0x0F;
    jump->long_form[1] = cond;
    jump->long_form[2] = 0xEF;
    jump->long_form[3] = 0xBE;
    jump->long_form[4] = 0xAD;
    jump->long_form[5] = 0xDE;

    jump->short_form[0] = cond - 0x10;
    jump->short_form[1] = 0xFF;

    jump->next_jump = NULL;
    jump->prev_jump = NULL;

    state->block_size += 6;

    state->curr_mem = create_mem_block_code(NULL, (union MemBlock*) jump, state->ctx);
}
// endregion

// region Utility

// endregion

// region Emit
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
        asm_emit_add_r64_i32(this_reg, constant, state);
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
            a_register = instr_fetch_reg(instr->a, this_reg, state);
            b_register = instr_fetch_reg(instr->b, this_reg, state);
            if (a_register == this_reg || b_register == this_reg) {
                if (a_register == this_reg) {
                    secondary_reg = b_register;
                } else {
                    secondary_reg = a_register;
                }
                asm_emit_add_r64_r64(this_reg, secondary_reg, state);
            } else {
                asm_emit_add_r64_r64(this_reg, b_register, state);
                asm_emit_mov_r64_r64(this_reg, a_register, state);
            }
            return;
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
        asm_emit_sub_r64_i32(this_reg, constant, state);
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
        if (store) asm_emit_setcc(instr->cmp, this_reg, state);
        asm_emit_cmp_r64_i32(cmp_with, constant, state);
        return;
    }
#endif
    if (store) asm_emit_setcc(instr->cmp, this_reg, state);
    asm_emit_cmp_r64_r64(a_register, b_register, state);
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
    asm_emit_xchg_r64_r64(state->swap_owner_of[entry_reg], this_reg, state);
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

void __attribute__((always_inline)) emit_get_attr(Instruction* instruction, struct AssemblerState* state) {
    struct GetAttrIR* instr = &instruction->ir_get_attr;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 obj_reg = instr_fetch_reg(instr->obj, RCX, state);

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
    asm_emit_mov_r64_i64(RAX, (uint64_t) hash_table_get_ptr, state);
    asm_emit_mov_r64_r64(RCX, obj_reg, state);
    asm_emit_mov_r64_i64(RDX, (uint64_t) instr->attr, state);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, state);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, state);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, state);
}

void __attribute__((always_inline)) emit_get_loc(Instruction* instruction, struct AssemblerState* state) {
    struct GetLocIR* instr = &instruction->ir_get_loc;
    OJIT_ASSERT(INSTR_TYPE(instr->loc) == ID_GET_ATTR_IR, "err");

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    unmark_register(this_reg, state);

    Register64 loc_reg = instr_fetch_reg(instr->loc, this_reg, state);

    asm_emit_mov_r64_ir64(this_reg, loc_reg, state);
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

    asm_emit_mov_ir64_r64(loc_reg, value_reg, state);
}

void __attribute__((always_inline)) emit_new_object(Instruction* instruction, struct AssemblerState* state) {
    struct NewObjectIR* instr = &instruction->ir_new_object;

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
    asm_emit_mov_r64_i64(RAX, (uint64_t) new_hash_table, state);
    asm_emit_mov_r64_i64(RCX, (uint64_t) state->jit_mem, state);

    if (state->used_registers[RCX]) asm_emit_push_r64(RCX, state);
    if (state->used_registers[RDX]) asm_emit_push_r64(RDX, state);
    if (state->used_registers[RAX]) asm_emit_push_r64(RAX, state);
}
// endregion

// region Emit Terminators
void __attribute__((always_inline)) emit_return(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct ReturnIR* ret = &terminator->ir_return;
    Register64 value_reg = instr_fetch_reg(ret->value, RAX, state);
    asm_emit_byte(0xC3, state);
    if (value_reg != RAX) {
        asm_emit_mov_r64_r64(RAX, value_reg, state);
    }
}

Register64 __attribute__((always_inline)) find_target_reg(bool* target_registers, Register64 suggestion, struct AssemblerState* state) {
    if (!target_registers[suggestion]) return suggestion;
    for (Register64 reg = 0; reg < 16; reg++) {
        if (!target_registers[reg] && !state->used_registers[reg]) {
            return reg;
        }
    }
    printf("Too many registers used concurrently");
    exit(-1);  // TODO register spilling
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
                    printf("something here also used the register the argument needs to go into\n");
                    exit(-1);  // TODO oh god not again
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
            asm_emit_mov_r64_r64(param_reg, argument_reg, state);
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

#ifdef OJIT_OPTIMIZATIONS
    if (INSTR_TYPE(cbranch->cond) == ID_CMP_IR) {
        if (!IS_ASSIGNED(GET_REG(cbranch->cond))) {
            asm_emit_jcc(IF_NOT_ZERO, cbranch->true_target, state);
            resolve_branch(cbranch->true_target, state);
            asm_emit_jcc(INV_CMP(cbranch->cond->ir_cmp.cmp), cbranch->false_target, state);
            resolve_branch(cbranch->false_target, state);
            emit_cmp(cbranch->cond, state, false);
            return;
        }
    }
#endif

    asm_emit_jcc(IF_NOT_ZERO, cbranch->true_target, state);
    resolve_branch(cbranch->true_target, state);
    asm_emit_jcc(IF_ZERO, cbranch->false_target, state);
    resolve_branch(cbranch->false_target, state);

    Register64 value_reg = instr_fetch_reg(cbranch->cond, NO_REG, state);
    asm_emit_test_r64_r64(value_reg, value_reg, state);
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

    struct BlockIR* block = func->first_block;
    while (block) {
        printf("    BLOCK @%p\n", block);

        FOREACH_INSTR(instr, block->first_instrs) {
            int i = get_var_num(instr, &var_names);
#ifdef OJIT_READABLE_IR
            if (instr->base.refs == 0) {
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
                case ID_NEW_OBJECT_IR: {
                    printf("$%i = NEW_OBJECT\n", i);
                    break;
                }
                case ID_GET_ATTR_IR: {
                    printf("$%i = GETATTR $%i\n", i, get_var_num(instr->ir_get_attr.obj, &var_names));
                    break;
                }
                case ID_GET_LOC_IR: {
                    printf("$%i = GETLOC $%i\n", i, get_var_num(instr->ir_get_loc.loc, &var_names));
                    break;
                }
                case ID_SET_LOC_IR: {
                    printf("$%i = SETLOC $%i, $%i\n", i, get_var_num(instr->ir_set_loc.loc, &var_names), get_var_num(instr->ir_set_loc.value, &var_names));
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
                printf("        BRANCH @%p\n", block->terminator.ir_branch.target);
                break;
            }
            case ID_CBRANCH_IR: {
                printf("        CBRANCH $%i (true: @%p, false: @%p)\n",
                       get_var_num(block->terminator.ir_cbranch.cond, &var_names),
                       block->terminator.ir_cbranch.true_target,
                       block->terminator.ir_cbranch.false_target);
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

        block = block->next_block;
    }
    destroy_mem_ctx(tmp_mem);
}
// endregion

// region Compile
void assign_function_parameters(struct FunctionIR* func) {
    struct BlockIR* first_block = func->first_block;
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


#define GET_RECORD(block) ((struct BlockRecord*) (block)->data)

struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback) {
#ifdef OJIT_OPTIMIZATIONS
    ojit_optimize_func(func, callback);
#endif
    dump_function(func);

    struct AssemblerState state;
    state.ctx = compiler_mem;
    state.jit_mem = create_mem_ctx(); // TODO bring this out

    size_t generated_size = 0;

    assign_function_parameters(func);

    struct BlockIR* block = func->first_block;
    while (block) {
        union MemBlock* end_mem = create_mem_block_code(NULL, NULL, compiler_mem);

        init_asm_state(&state, block, end_mem, callback);

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

        generated_size += state.block_size;

        struct BlockRecord* record = ojit_alloc(compiler_mem, sizeof(struct BlockRecord));
        record->end_mem = end_mem;
        record->max_offset_from_end = generated_size;
        record->actual_offset_from_end = 0;
        block->data = record;

        block = block->next_block;
    }

    uint8_t* func_mem = ojit_alloc(compiler_mem, generated_size);
    uint8_t* end_ptr = func_mem + generated_size;
    uint8_t* write_ptr = end_ptr;

    struct MemBlockJump* last_visited_jump = NULL;
    block = func->last_block;
    while (block) {
        struct BlockRecord* record = GET_RECORD(block);
        union MemBlock* curr_mem_block = record->end_mem;
        while (curr_mem_block) {
            if (curr_mem_block->base.is_code) {
                write_ptr -= curr_mem_block->base.len;
                ojit_memcpy(write_ptr, curr_mem_block->code.code + (512 - curr_mem_block->base.len), curr_mem_block->base.len);
            } else {
                struct BlockRecord* target_record = GET_RECORD(curr_mem_block->jump.target);
                uint32_t curr_offset = end_ptr - write_ptr;
                int32_t jump_dist;
                if (target_record->actual_offset_from_end != 0) { // TODO account for the extra bytes
                    jump_dist = target_record->actual_offset_from_end - curr_offset;
                } else {
                    jump_dist = target_record->max_offset_from_end - curr_offset;
                }
#ifdef OJIT_OPTIMIZATIONS
                if (jump_dist <= 256 && jump_dist >= -256) {
                    curr_mem_block->jump.is_short = true;
                    write_ptr -= 2;
                    ojit_memcpy(write_ptr, curr_mem_block->jump.short_form, 2);
                } else {
                    curr_mem_block->jump.is_short = false;
                    write_ptr -= curr_mem_block->base.len;
                    ojit_memcpy(write_ptr, curr_mem_block->jump.long_form + (6 - curr_mem_block->jump.base.len), curr_mem_block->base.len);
                }
#else
                curr_mem_block->jump.is_short = false;
                write_ptr -= curr_mem_block->base.len;
                ojit_memcpy(write_ptr, curr_mem_block->jump.long_form + (6 - curr_mem_block->jump.base.len), curr_mem_block->base.len);
#endif
                curr_mem_block->jump.offset_from_end = end_ptr - write_ptr;
                curr_mem_block->jump.next_jump = last_visited_jump;
                if (last_visited_jump) last_visited_jump->prev_jump = (struct MemBlockJump*) curr_mem_block;
                last_visited_jump = (struct MemBlockJump*) curr_mem_block;
            }
            curr_mem_block = curr_mem_block->base.prev_block;
        }
        record->actual_offset_from_end = end_ptr - write_ptr;
        block = block->prev_block;
    }

    struct MemBlockJump* curr_jump = last_visited_jump;
    while (curr_jump) {
        if (curr_jump->is_short) {
            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + 2;
            uint32_t jump_dist = (curr_jump->offset_from_end - 2) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
            *(ptr - 1) = jump_dist & 0xFF;
        } else {
            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + curr_jump->base.len;
            uint32_t jump_dist = (curr_jump->offset_from_end - curr_jump->base.len) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
            *(ptr - 1) = (jump_dist >> 24) & 0xFF;
            *(ptr - 2) = (jump_dist >> 16) & 0xFF;
            *(ptr - 3) = (jump_dist >> 8) & 0xFF;
            *(ptr - 4) = (jump_dist >> 0) & 0xFF;
        }
        curr_jump = curr_jump->next_jump;
    }

    uint32_t final_size = end_ptr - write_ptr;

    return (struct CompiledFunction) {write_ptr, final_size};
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
