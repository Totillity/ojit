#ifndef OJIT_EMIT_X64_H
#define OJIT_EMIT_X64_H

#include "../ojit_mem.h"
#include "compiler_records.h"

// region Emit Assembly
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblerState* state) {
    if (state->curr_segment->base.max_size >= 512) {
        state->curr_segment = create_segment_code(state->label_segment, state->curr_segment, state->ctx);
    }
    state->curr_segment->base.max_size++;
    state->block_size++;
    state->curr_segment->code.code[512 - state->curr_segment->base.max_size] = byte;
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

void __attribute__((always_inline)) asm_emit_shr_r64_i8(Register64 source, uint8_t constant, struct AssemblerState* state) {
    asm_emit_int8(constant, state);
    asm_emit_byte(MODRM(0b11, 5, source & 0b0111), state);
    asm_emit_byte(0xC1, state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_and_r8_i8(Register64 source, uint8_t constant, struct AssemblerState* state) {
    asm_emit_int8(constant, state);
    asm_emit_byte(MODRM(0b11, 4, source & 0b0111), state);
    asm_emit_byte(0x80, state);
    if ((source >> 3) & 0b0001) {
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), state);
    }
}


void __attribute__((always_inline)) asm_emit_setcc(enum Comparison cond, enum Register64 reg, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, 0, reg & 0b0111), state);
    asm_emit_byte(cond + 0x10, state);
    asm_emit_byte(0x0F, state);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0, 0, 0, 1), state);
    }
}

void __attribute__((always_inline)) asm_emit_jmp(Segment* jump_after, struct AssemblerState* state) {
//#ifdef OJIT_OPTIMIZATIONS
//    if (target->prev_block == state->block) return;
//#endif
    struct SegmentJump* jump = (struct SegmentJump*) create_mem_block_jump(state->label_segment, state->curr_segment, state->ctx);
    jump->jump_to = (struct SegmentLabel*) jump_after;
    jump->base.max_size = 5;
    jump->long_form[0] = 0xE9;
    jump->long_form[1] = 0xEF;
    jump->long_form[2] = 0xBE;
    jump->long_form[3] = 0xAD;
    jump->long_form[4] = 0xDE;

    jump->short_form[0] = 0xEB;
    jump->short_form[1] = 0xFF;

    state->block_size += 5;

    state->curr_segment = create_segment_code(state->label_segment, (Segment*) jump, state->ctx);
}

void __attribute__((always_inline)) asm_emit_jcc(enum Comparison cond, Segment* jump_after, struct AssemblerState* state) {
//#ifdef OJIT_OPTIMIZATIONS
//    if (target->prev_block == state->block) return;
//#endif
    struct SegmentJump* jump = (struct SegmentJump*) create_mem_block_jump(state->label_segment, state->curr_segment, state->ctx);
    jump->jump_to = (struct SegmentLabel*) jump_after;
    jump->base.max_size = 6;
    jump->long_form[0] = 0x0F;
    jump->long_form[1] = cond;
    jump->long_form[2] = 0xEF;
    jump->long_form[3] = 0xBE;
    jump->long_form[4] = 0xAD;
    jump->long_form[5] = 0xDE;

    jump->short_form[0] = cond - 0x10;
    jump->short_form[1] = 0xFF;

    state->block_size += 6;

    state->curr_segment = create_segment_code(state->label_segment, (Segment*) jump, state->ctx);
}
// endregion

#endif //OJIT_EMIT_X64_H
