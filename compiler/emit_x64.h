#ifndef OJIT_EMIT_X64_H
#define OJIT_EMIT_X64_H

#include "../ojit_mem.h"
#include "compiler_records.h"

// region Emit Assembly
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblyWriter* writer) {
    if (writer->curr->base.max_size >= 512) {
        writer->curr = create_segment_code(writer->label, writer->curr, writer->write_mem);
    }
    writer->curr->code.code[512 - ++writer->curr->base.max_size] = byte;
}

void __attribute__((always_inline)) asm_emit_int8(uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_byte(constant, writer);
}

void __attribute__((always_inline)) asm_emit_int32(uint32_t constant, struct AssemblyWriter* writer) {
    asm_emit_byte((constant >> 24) & 0xFF, writer);
    asm_emit_byte((constant >> 16) & 0xFF, writer);
    asm_emit_byte((constant >>  8) & 0xFF, writer);
    asm_emit_byte((constant >>  0) & 0xFF, writer);
}

void __attribute__((always_inline)) asm_emit_int64(uint64_t constant, struct AssemblyWriter* writer) {
    asm_emit_byte((constant >> 56) & 0xFF, writer);
    asm_emit_byte((constant >> 48) & 0xFF, writer);
    asm_emit_byte((constant >> 40) & 0xFF, writer);
    asm_emit_byte((constant >> 32) & 0xFF, writer);
    asm_emit_byte((constant >> 24) & 0xFF, writer);
    asm_emit_byte((constant >> 16) & 0xFF, writer);
    asm_emit_byte((constant >>  8) & 0xFF, writer);
    asm_emit_byte((constant >>  0) & 0xFF, writer);
}

void __attribute__((always_inline)) asm_emit_test_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x85, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_xchg_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x87, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_xor_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), writer);
    asm_emit_byte(0x33, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r64_ir64(Register64 dest, Register64 in_source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b00, dest & 0b111, in_source & 0b0111), writer);
    asm_emit_byte(0x8B, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, in_source >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_ir64_r64(Register64 in_dest, Register64 source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b00, source & 0b111, in_dest & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, in_dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r64_i64(Register64 dest, uint64_t constant, struct AssemblyWriter* writer) {
    // TODO look into movzx instruction
#ifdef OJIT_OPTIMIZATIONS
    if (constant == 0) {
        asm_emit_xor_r64_r64(dest, dest, writer);
        return;
    }
    if (constant <= UINT32_MAX) {
        asm_emit_int32(constant, writer);
        asm_emit_byte(0xB8 + (dest & 0b0111), writer);
        if (dest & 0b1000) {
            asm_emit_byte(REX(0b0, 0b0, 0b0, dest >> 3 & 0b0001), writer);
        }
        return;
    }
#endif
    asm_emit_int64(constant, writer);
    asm_emit_byte(0xB8 + (dest & 0b0111), writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, dest >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_call_r64(Register64 reg, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, 0b10, reg & 0b0111), writer);
    asm_emit_byte(0xFF, writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_pop_r64(Register64 reg, struct AssemblyWriter* writer) {
    asm_emit_byte(0x58 + (reg & 0b0111), writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_push_r64(Register64 reg, struct AssemblyWriter* writer) {
    asm_emit_byte(0x50 + (reg & 0b0111), writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_add_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), writer);
    asm_emit_byte(0x01, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_add_r64_i32(Register64 source, uint32_t constant, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, writer);
        asm_emit_byte(MODRM(0b11, 0, source & 0b0111), writer);
        asm_emit_byte(0x83, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, writer);
        asm_emit_byte(0x05, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), writer);
        return;
    }
#endif
    asm_emit_int32(constant, writer);
    asm_emit_byte(MODRM(0b11, 0, source & 0b0111), writer);
    asm_emit_byte(0x81, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_sub_r64_r64(Register64 dest, Register64 source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x29, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_sub_r64_i32(Register64 source, uint32_t constant, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, writer);
        asm_emit_byte(MODRM(0b11, 5, source & 0b0111), writer);
        asm_emit_byte(0x83, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, writer);
        asm_emit_byte(0x2D, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), writer);
        return;
    }
#endif
    asm_emit_int32(constant, writer);
    asm_emit_byte(MODRM(0b11, 5, source & 0b0111), writer);
    asm_emit_byte(0x81, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_r64(Register64 a, Register64 b, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, b & 0b111, a & 0b0111), writer);
    asm_emit_byte(0x39, writer);
    asm_emit_byte(REX(0b1, b >> 3 & 0b1, 0b0, a >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_i32(Register64 source, uint32_t constant, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, writer);
        asm_emit_byte(MODRM(0b11, 7, source & 0b0111), writer);
        asm_emit_byte(0x83, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, writer);
        asm_emit_byte(0x3D, writer);
        asm_emit_byte(REX(0b1, 0b0, 0b0, 0b0), writer);
        return;
    }
#endif
    asm_emit_int32(constant, writer);
    asm_emit_byte(MODRM(0b11, 7, source & 0b0111), writer);
    asm_emit_byte(0x81, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_shr_r64_i8(Register64 source, uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_int8(constant, writer);
    asm_emit_byte(MODRM(0b11, 5, source & 0b0111), writer);
    asm_emit_byte(0xC1, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_and_r8_i8(Register64 source, uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_int8(constant, writer);
    asm_emit_byte(MODRM(0b11, 4, source & 0b0111), writer);
    asm_emit_byte(0x80, writer);
    if ((source >> 3) & 0b0001) {
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
    }
}


void __attribute__((always_inline)) asm_emit_setcc(enum Comparison cond, enum Register64 reg, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, 0, reg & 0b0111), writer);
    asm_emit_byte(cond + 0x10, writer);
    asm_emit_byte(0x0F, writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0, 0, 0, 1), writer);
    }
}

void __attribute__((always_inline)) asm_emit_jmp(Segment* jump_after, struct AssemblyWriter* writer) {
//#ifdef OJIT_OPTIMIZATIONS
//    if (target->prev_segment == state->block) return;
//#endif
    struct SegmentJump* jump = (struct SegmentJump*) create_mem_block_jump(writer->label, writer->curr, writer->write_mem);
    jump->jump_to = (struct SegmentLabel*) jump_after;
    jump->base.max_size = 5;
    jump->long_form[0] = 0xE9;
    jump->long_form[1] = 0xEF;
    jump->long_form[2] = 0xBE;
    jump->long_form[3] = 0xAD;
    jump->long_form[4] = 0xDE;

    jump->short_form[0] = 0xEB;
    jump->short_form[1] = 0xFF;

    writer->curr = create_segment_code(writer->label, (Segment*) jump, writer->write_mem);
}

void __attribute__((always_inline)) asm_emit_jcc(enum Comparison cond, Segment* jump_after, struct AssemblyWriter* writer) {
//#ifdef OJIT_OPTIMIZATIONS
//    if (target->prev_segment == state->block) return;
//#endif
    struct SegmentJump* jump = (struct SegmentJump*) create_mem_block_jump(writer->label, writer->curr, writer->write_mem);
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

    writer->curr = create_segment_code(writer->label, (Segment*) jump, writer->write_mem);
}
// endregion

#endif //OJIT_EMIT_X64_H
