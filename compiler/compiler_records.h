#ifndef OJIT_COMPILER_RECORDS_H
#define OJIT_COMPILER_RECORDS_H

#include <stdint.h>
#include <stdbool.h>

#include "../asm_ir.h"

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

#endif //OJIT_COMPILER_RECORDS_H
