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

void __attribute__((always_inline)) asm_emit_test_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x85, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_xchg_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x87, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_xor_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), writer);
    asm_emit_byte(0x33, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r32_r32(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    if ((source >> 3 & 0b1) || (dest >> 3 & 0b1))
        asm_emit_byte(REX(0b0, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r32_ir32(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x8B, writer);
    if ((base >> 3 & 0b1) || (dest >> 3 & 0b1))
        asm_emit_byte(REX(0b0, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_ir32_r32(enum Registers base, uint8_t offset, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, source, base & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    if ((base >> 3 & 0b1) || (source >> 3 & 0b1))
        asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (dest == source) return;
#endif
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_load_with_offset(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x8B, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_store_with_offset(enum Registers base, uint8_t offset, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, source, base & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_ir64_r64(enum Registers in_dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b00, source & 0b111, in_dest & 0b0111), writer);
    asm_emit_byte(0x89, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, in_dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_mov_r64_i64(enum Registers dest, uint64_t constant, struct AssemblyWriter* writer) {
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

void __attribute__((always_inline)) asm_emit_xchg_r64_ir64(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x87, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_call_r64(enum Registers reg, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, 0b10, reg & 0b0111), writer);
    asm_emit_byte(0xFF, writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_call_ir64(enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, 2, base & 0b0111), writer);
    asm_emit_byte(0xFF, writer);
}

void __attribute__((always_inline)) asm_emit_pop_r64(enum Registers reg, struct AssemblyWriter* writer) {
    asm_emit_byte(0x58 + (reg & 0b0111), writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_push_r64(enum Registers reg, struct AssemblyWriter* writer) {
    asm_emit_byte(0x50 + (reg & 0b0111), writer);
    if (reg & 0b1000) {
        asm_emit_byte(REX(0b0, 0b0, 0b0, reg >> 3 & 0b0001), writer);
    }
}

void __attribute__((always_inline)) asm_emit_and_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), writer);
    asm_emit_byte(0x21, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_and_r64_ir64(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x23, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_and_ir64_r64(enum Registers base, uint8_t offset, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, source, base & 0b0111), writer);
    asm_emit_byte(0x21, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_add_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), writer);
    asm_emit_byte(0x01, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_add_r64_ir64(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x03, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_add_ir64_r64(enum Registers base, uint8_t offset, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, source, base & 0b0111), writer);
    asm_emit_byte(0x01, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_add_r64_i32(enum Registers source, uint32_t constant, struct AssemblyWriter* writer) {
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

void __attribute__((always_inline)) asm_emit_sub_r64_r64(enum Registers dest, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), writer);
    asm_emit_byte(0x29, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_sub_r64_ir64(enum Registers dest, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, dest, base & 0b0111), writer);
    asm_emit_byte(0x2B, writer);
    asm_emit_byte(REX(0b1, dest >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_sub_ir64_r64(enum Registers base, uint8_t offset, enum Registers source, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, source, base & 0b0111), writer);
    asm_emit_byte(0x29, writer);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_sub_r64_i32(enum Registers source, uint32_t constant, struct AssemblyWriter* writer) {
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

void __attribute__((always_inline)) asm_emit_cmp_r64_r64(enum Registers a, enum Registers b, struct AssemblyWriter* writer) {
    asm_emit_byte(MODRM(0b11, b & 0b111, a & 0b0111), writer);
    asm_emit_byte(0x39, writer);
    asm_emit_byte(REX(0b1, b >> 3 & 0b1, 0b0, a >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_ir64(enum Registers a, enum Registers base, uint8_t offset, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, base & 0b111, a & 0b0111), writer);
    asm_emit_byte(0x3B, writer);
    asm_emit_byte(REX(0b1, base >> 3 & 0b1, 0b0, a >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_cmp_ir64_r64(enum Registers base, uint8_t offset, enum Registers b, struct AssemblyWriter* writer) {
    asm_emit_int8(offset, writer);
    asm_emit_byte(MODRM(0b01, b & 0b0111, base & 0b111), writer);
    asm_emit_byte(0x3B, writer);
    asm_emit_byte(REX(0b1, b >> 3 & 0b1, 0b0, base >> 3 & 0b1), writer);
}

void __attribute__((always_inline)) asm_emit_cmp_r64_i32(enum Registers source, uint32_t constant, struct AssemblyWriter* writer) {
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

void __attribute__((always_inline)) asm_emit_cmp_r32_i32(enum Registers source, uint32_t constant, struct AssemblyWriter* writer) {
#ifdef OJIT_OPTIMIZATIONS
    if (constant <= UINT8_MAX) {
        asm_emit_int8(constant, writer);
        asm_emit_byte(MODRM(0b11, 7, source & 0b0111), writer);
        asm_emit_byte(0x83, writer);
        if (source >> 3 & 0b0001) asm_emit_byte(REX(0b0, 0b0, 0b0, source >> 3 & 0b0001), writer);
        return;
    }
    if (source == RAX) {
        asm_emit_int32(constant, writer);
        asm_emit_byte(0x3D, writer);
        return;
    }
#endif
    asm_emit_int32(constant, writer);
    asm_emit_byte(MODRM(0b11, 7, source & 0b0111), writer);
    asm_emit_byte(0x81, writer);
    if (source >> 3 & 0b0001) asm_emit_byte(REX(0b0, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_shr_r64_i8(enum Registers source, uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_int8(constant, writer);
    asm_emit_byte(MODRM(0b11, 5, source & 0b0111), writer);
    asm_emit_byte(0xC1, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_sar_r64_i8(enum Registers source, uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_int8(constant, writer);
    asm_emit_byte(MODRM(0b11, 7, source & 0b0111), writer);
    asm_emit_byte(0xC1, writer);
    asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
}

void __attribute__((always_inline)) asm_emit_and_r8_i8(enum Registers source, uint8_t constant, struct AssemblyWriter* writer) {
    asm_emit_int8(constant, writer);
    asm_emit_byte(MODRM(0b11, 4, source & 0b0111), writer);
    asm_emit_byte(0x80, writer);
    if ((source >> 3) & 0b0001) {
        asm_emit_byte(REX(0b1, 0b0, 0b0, source >> 3 & 0b0001), writer);
    }
}


void __attribute__((always_inline)) asm_emit_setcc(enum Comparison cond, enum Registers reg, struct AssemblyWriter* writer) {
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

void __attribute__((always_inline)) asm_emit_mov(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_mov_r64_r64(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_load_with_offset(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_store_with_offset(RBP, dest.offset * 8, source.reg, writer);
    } else {
        asm_emit_store_with_offset(RBP, dest.offset * 8, TMP_1_REG, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_mov32(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_mov_r32_r32(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_mov_r32_ir32(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_mov_ir32_r32(RBP, dest.offset * 8, source.reg, writer);
    } else {
        asm_emit_mov_ir32_r32(RBP, dest.offset * 8, TMP_1_REG, writer);
        asm_emit_mov_r32_ir32(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_xchg(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_xchg_r64_r64(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_xchg_r64_ir64(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_xchg_r64_ir64(source.reg, RBP, dest.offset * 8, writer);
    } else {
        asm_emit_store_with_offset(RBP, source.offset * 8, TMP_1_REG, writer);
        asm_emit_xchg_r64_ir64(TMP_1_REG, RBP, dest.offset * 8, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}


void __attribute__((always_inline)) asm_emit_call(VLoc callee, struct AssemblyWriter* writer) {
    if (callee.is_reg) {
        asm_emit_call_r64(callee.reg, writer);
    } else {
        asm_emit_call_ir64(RBP, callee.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_and(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_and_r64_r64(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_and_r64_ir64(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_and_ir64_r64(RBP, dest.offset * 8, source.reg, writer);
    } else {
        asm_emit_and_ir64_r64(RBP, dest.offset * 8, TMP_1_REG, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_add(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_add_r64_r64(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_add_r64_ir64(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_add_ir64_r64(RBP, dest.offset * 8, source.reg, writer);
    } else {
        asm_emit_add_ir64_r64(RBP, dest.offset * 8, TMP_1_REG, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_sub(VLoc dest, VLoc source, struct AssemblyWriter* writer) {
    if (dest.is_reg && source.is_reg) {
        asm_emit_sub_r64_r64(dest.reg, source.reg, writer);
    } else if (dest.is_reg) {
        asm_emit_sub_r64_ir64(dest.reg, RBP, source.offset * 8, writer);
    } else if (source.is_reg) {
        asm_emit_sub_ir64_r64(RBP, dest.offset * 8, source.reg, writer);
    } else {
        asm_emit_sub_ir64_r64(RBP, dest.offset * 8, TMP_1_REG, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, source.offset * 8, writer);
    }
}

void __attribute__((always_inline)) asm_emit_cmp(VLoc a, VLoc b, struct AssemblyWriter* writer) {
    if (a.is_reg && b.is_reg) {
        asm_emit_cmp_r64_r64(a.reg, b.reg, writer);
    } else if (a.is_reg) {
        asm_emit_cmp_r64_ir64(a.reg, RBP, b.offset * 8, writer);
    } else if (b.is_reg) {
        asm_emit_cmp_ir64_r64(RBP, a.offset * 8, b.reg, writer);
    } else {
        asm_emit_cmp_ir64_r64(RBP, b.offset * 8, TMP_1_REG, writer);
        asm_emit_load_with_offset(TMP_1_REG, RBP, a.offset * 8, writer);
    }
}

void __attribute__((always_inline)) map_registers(VLoc** map_from, VLoc** map_to, uint32_t rows, struct AssemblyWriter* writer) {
    VLoc* moves_from[rows];
    VLoc* moves_to[rows];
    for (int i = 0; i < rows; i++) {
        moves_from[i] = NULL;
        moves_to[i] = NULL;
    }

    for (int i = rows-1; i >= 0; i--) {
        bool must_xchg = false;
        VLoc* from = map_from[i];
        VLoc* to = map_to[i];
        for (int k = i - 1; k >= 0; k--) {
            if (loc_equal(*map_to[k], *from)) {
                must_xchg = true;
                break;
            }
        }
        VLoc* loc_into = to;
        for (int k = rows - 1; k >= 0; k--) {
            if (moves_from[k] && loc_equal(*to, *moves_from[k])) {
                loc_into = moves_to[k];
                moves_from[k] = moves_to[k] = NULL;
                break;
            }
        }
        if (must_xchg) {
            for (int k = rows - 1; k >= 0; k--) {
                if (!moves_from[k]) {
                    moves_from[k] = from;
                    moves_to[k] = loc_into;
                    break;
                }
            }
            printf("emitted xchg\n");
            asm_emit_xchg(*loc_into, *from, writer);
        } else {
            asm_emit_mov(*loc_into, *from, writer);
        }
    }
}

#endif //OJIT_EMIT_X64_H
