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

    union u_Segment* prev_segment;
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

struct AssemblyWriter {
    Segment* label;
    Segment* curr;
    MemCtx* write_mem;
};

struct AssemblerState {
    struct AssemblyWriter writer;
    uint8_t curr_num_vars;
    uint8_t max_num_vars;

    struct BlockIR* block;

    Segment* errs_label;
    Segment* err_return_label;

    bool used_registers[16];
    enum Registers swap_owner_of[16];
    enum Registers swap_contents[16];
    Instruction* curr_tmp_1_user;
    Instruction* curr_tmp_2_user;

    MemCtx* jit_mem;
    struct GetFunctionCallback callback;
};

void init_asm_state(struct AssemblerState* state, struct BlockIR* block, Segment* label, Segment* curr) {
    state->block = block;
    state->writer.curr = curr;
    state->writer.label = label;

    state->curr_num_vars = 0;
    state->max_num_vars = 0;

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
    state->used_registers[TMP_1_REG] = false;
    state->used_registers[TMP_2_REG] = false;
    state->used_registers[R14]         = true;
    state->used_registers[R15]         = true;

    state->curr_tmp_1_user = NULL;
    state->curr_tmp_2_user = NULL;

    for (int i = 0; i < 16; i++) {
        state->swap_owner_of[i] = i;
        state->swap_contents[i] = i;
    }
}


uint8_t state_alloc_var(struct AssemblerState* state) {
    uint8_t num = state->curr_num_vars++;
    if (state->curr_num_vars > state->max_num_vars) {
        state->max_num_vars = state->curr_num_vars;
    }
    return num;
}

Segment* create_segment_label(Segment* prev_block, Segment* next_block, MemCtx* ctx) {
    struct SegmentLabel* segment = ojit_alloc(ctx, sizeof(Segment));
    segment->base.max_size = 0;
    segment->base.final_size = 0;
    segment->base.type = SEGMENT_LABEL;

    segment->base.prev_segment = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_segment = (Segment*) segment;
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

    segment->base.prev_segment = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_segment = (Segment*) segment;
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

    segment->base.prev_segment = prev_block;
    segment->base.next_segment = next_block;
    if (next_block) {
        next_block->base.prev_segment = (Segment*) segment;
    }
    if (prev_block) {
        prev_block->base.next_segment = (Segment*) segment;
    }
    return (Segment*) segment;
}

#endif //OJIT_COMPILER_RECORDS_H
