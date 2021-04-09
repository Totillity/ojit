#ifndef OJIT_COMPILER_RECORDS_H
#define OJIT_COMPILER_RECORDS_H

#include <stdint.h>
#include <stdbool.h>

#include "../asm_ir.h"

enum SegmentType {
    SEGMENT_CODE,
    SEGMENT_JUMP,
    SEGMENT_LABEL,
};

struct SegmentBase {
    enum SegmentType type;
    uint32_t max_size;
    uint32_t final_size;
    uint32_t offset_from_start;

    union u_Segment* prev_block;
    union u_Segment* next_segment;
};

struct SegmentCode {
    struct SegmentBase base;
    uint8_t code[512];
};

struct SegmentLabel {
    struct SegmentBase base;
};

struct SegmentJump {
    struct SegmentBase base;
    uint8_t short_form[2];
    uint8_t long_form[6];
    struct SegmentLabel* jump_to;
};

typedef union u_Segment {
    struct SegmentBase base;
    struct SegmentCode code;
    struct SegmentJump jump;
    struct SegmentLabel label;
} Segment;

struct AssemblerState {
    struct BlockIR* block;

    Segment* label_segment;
    Segment* curr_segment;

    bool used_registers[16];
    enum Register64 swap_owner_of[16];
    enum Register64 swap_contents[16];

    MemCtx* jit_mem;
    MemCtx* ctx;
    struct GetFunctionCallback callback;
};

void init_asm_state(struct AssemblerState* state, struct BlockIR* block, Segment* curr_mem, Segment* label_segment) {
    state->block = block;
    state->curr_segment = curr_mem;
    state->label_segment = label_segment;

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
}

Segment* create_segment_label(Segment* prev_block, Segment* next_block, MemCtx* ctx) {
    struct SegmentLabel* segment = ojit_alloc(ctx, sizeof(Segment));
    segment->base.max_size = 0;
    segment->base.final_size = 0;
    segment->base.type = SEGMENT_LABEL;

    segment->base.prev_block = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_block = (Segment*) segment;
    }
    if (prev_block) {
        prev_block->base.next_segment = (Segment*) segment;
    }

    return (Segment*) segment;
}

Segment* create_segment_code(Segment* prev_block, Segment* next_block, MemCtx* ctx) {
    struct SegmentCode* segment = ojit_alloc(ctx, sizeof(Segment));
    segment->base.max_size = 0;
    segment->base.final_size = 0;
    segment->base.type = SEGMENT_CODE;

    segment->base.prev_block = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_block = (Segment*) segment;
    }
    if (prev_block) {
        prev_block->base.next_segment = (Segment*) segment;
    }
    return (Segment*) segment;
}

Segment* create_mem_block_jump(Segment* prev_block, Segment* next_block, MemCtx* ctx) {
    struct SegmentJump* segment = &((Segment*) ojit_alloc(ctx, sizeof(Segment)))->jump;
    segment->base.max_size = 0;
    segment->base.final_size = 0;
    segment->base.type = SEGMENT_JUMP;

    segment->base.prev_block = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_block = (Segment*) segment;
    }
    if (prev_block) {
        prev_block->base.next_segment = (Segment*) segment;
    }
    return (Segment*) segment;
}

#endif //OJIT_COMPILER_RECORDS_H
