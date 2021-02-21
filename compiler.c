#include <assert.h>
#include <stdio.h>

#include "ojit_err.h"
#include "ojit_state.h"
#include "compiler.h"


// region Compilation
#define MAXIMUM_INSTRUCTION_SIZE (15)

// region Utility Functions & Macros
Register64 get_unused(const bool* registers) {
    for (int i = 0; i < 16; i++) {
        if (!registers[i]) return i;
    }
    return NO_REG;
}
// endregion


//struct MemBlock {
//    uint8_t* front_ptr;
//    struct MemBlock* next_block;
//    uint8_t mem[256];
//};


struct AssemblerState {
    uint8_t* front_ptr;
    bool used_registers[16];
    LAList* newest_block;
    MemCtx* ctx;
};


void init_asm_state(struct AssemblerState* state, LAList* init_mem) {
    state->front_ptr = &init_mem->mem[LALIST_BLOCK_SIZE];

    state->used_registers[RAX] = false;
    state->used_registers[RCX] = false;
    state->used_registers[RDX] = false;
    state->used_registers[RBX] = false;
    state->used_registers[NO_REG] = true;
    state->used_registers[SPILLED_REG] = true;
    state->used_registers[RSI] = false;
    state->used_registers[RDI] = false;
    state->used_registers[R8]  = false;
    state->used_registers[R9]  = false;
    state->used_registers[R10] = false;
    state->used_registers[R11] = false;
    state->used_registers[TMP_1_REG] = true;
    state->used_registers[TMP_2_REG] = true;
    state->used_registers[R14] = false;
    state->used_registers[R15] = false;

    state->newest_block = init_mem;
}


#define GET_REG(value) ((value)->base.reg)
#define SET_REG(value, regi) ((value)->base.reg = (regi))
#define REG_IS_MARKED(reg) (state->used_registers[(reg)])


// region asm utility
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblerState* state) {
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

void __attribute__((always_inline)) asm_emit_mov_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x89, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_mov_r64_i64(Register64 dest, uint64_t constant, struct AssemblerState* state) {
    if (constant <= UINT32_MAX) {
        asm_emit_int32(constant, state);
        asm_emit_byte(0xB8 + (dest & 0b0111), state);
        if (dest & 0b1000) {
            asm_emit_byte(REX(0b0, 0b0, 0b0, dest >> 3 & 0b0001), state);
        }
    } else {
        asm_emit_int64(constant, state);
        asm_emit_byte(0xB8 + (dest & 0b0111), state);
        asm_emit_byte(REX(0b1, 0b0, 0b0, dest >> 3 & 0b0001), state);
    }
}

void __attribute__((always_inline)) asm_emit_add_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b0111, dest & 0b0111), state);
    asm_emit_byte(0x01, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

void __attribute__((always_inline)) asm_emit_sub_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
    asm_emit_byte(0x29, state);
    asm_emit_byte(REX(0b1, source >> 3 & 0b1, 0b0, dest >> 3 & 0b1), state);
}

// endregion

void __attribute__((always_inline)) mark_register(Register64 reg, struct AssemblerState* state) {
    assert(state->used_registers[reg] == false);
    state->used_registers[reg] = true;
}

void __attribute__((always_inline)) unmark_register(Register64 reg, struct AssemblerState* state) {
    assert(state->used_registers[reg] == true);
    state->used_registers[reg] = false;
}

union InstructionIR* __attribute__((always_inline)) instr_ensure(union InstructionIR* instr, struct AssemblerState* state) {
    if (IS_PARAM_REG(GET_REG(instr))) {
        SET_REG(instr, NORMALIZE_REG(GET_REG(instr)));
        mark_register(GET_REG(instr), state);
    }
    return instr;
}

void __attribute__((always_inline)) instr_assign_reg(union InstructionIR* instr, Register64 reg) {
    assert(!IS_ASSIGNED(GET_REG(instr)));
    SET_REG(instr, reg);
}

//size_t __attribute__((always_inline)) get_block_size(struct MemBlock* block) {
//    return 256 - (block->front_ptr - block->mem);
//}

size_t __attribute__((always_inline)) check_new_block(struct AssemblerState* state) {
    if (state->front_ptr - state->newest_block->mem < (MAXIMUM_INSTRUCTION_SIZE + 1)) {
        state->newest_block->len = LALIST_BLOCK_SIZE - (state->front_ptr - state->newest_block->mem);
        state->newest_block = lalist_grow(state->ctx, NULL, state->newest_block);
        state->front_ptr = &state->newest_block->mem[LALIST_BLOCK_SIZE];
        return state->newest_block->len;
    }
    return 0;
}

void __attribute__((always_inline)) emit_return(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct ReturnIR* ret = &terminator->ir_return;
    // BTW, PARAM_NO_REG shouldn't be possible to ever make
    if (IS_ASSIGNED(ret->value->base.reg)) {
        // if this just returns a parameter (which could already be assigned), then move the parameter to RAX to return it
        Register64 value_reg = GET_REG(instr_ensure(ret->value, state));

        asm_emit_byte(0xC3, state);
        asm_emit_mov_r64_r64(RAX, value_reg, state);
    } else {
        instr_assign_reg(ret->value, RAX);
        mark_register(RAX, state);

        asm_emit_byte(0xC3, state);
    }
}

void __attribute__((always_inline)) emit_branch(union TerminatorIR* terminator, struct AssemblerState* state) {
    struct BranchIR* branch = &terminator->ir_branch;

    asm_emit_int32(0xEFBEADDE, state);  // comeback to this later after I've stitched everything together
    asm_emit_byte(0x89, state);

    LAList* instrs = branch->target->first_instrs;
    size_t instrs_index = 0;
    for (int k = 0; k < branch->argument_count; k++) {
        IRValue argument = branch->arguments[k];
        if (instrs_index > (LALIST_BLOCK_SIZE - sizeof(IRValue))) {
            instrs = instrs->next;
        }
        struct ParameterIR* param = (struct ParameterIR*) &instrs->mem[instrs_index];
        instrs_index += sizeof(IRValue);
        if (param->base.id != ID_PARAMETER_IR) {
            printf("Too many arguments");
            exit(-1);  // TODO programmer error
        }

        Register64 param_reg = GET_REG(param);
        Register64 argument_reg = GET_REG(instr_ensure(argument, state));
        bool param_assigned = IS_ASSIGNED(param_reg);
        bool arg_assigned = IS_ASSIGNED(argument_reg);

        if (param_assigned && arg_assigned) {
            if (REG_IS_MARKED(param_reg)) {
                printf("something here also used the register the argument needs to go into");
                exit(-1);
            }
            asm_emit_mov_r64_r64(param_reg, argument_reg, state);
        } else if (param_assigned) {
            instr_assign_reg(argument, param_reg);  // TODO we need to check this
            mark_register(GET_REG(argument), state);
        } else if (arg_assigned) {
            instr_assign_reg((union InstructionIR*) param, AS_PARAM_REG(argument_reg));
        } else {
            Register64 new_reg = get_unused(state->used_registers);
            if (new_reg == NO_REG) {
                printf("Too many registers used concurrently");
                exit(-1);  // TODO register spilling
            }
            instr_assign_reg(argument, new_reg);
            mark_register(GET_REG(argument), state);
            instr_assign_reg((union InstructionIR*) param, AS_PARAM_REG(new_reg));
        }
    }
}

void __attribute__((always_inline)) emit_int(union InstructionIR* instruction, struct AssemblerState* state) {
    struct IntIR* instr = &instruction->ir_int;
    if (IS_ASSIGNED(GET_REG(instr))) {
        asm_emit_mov_r64_i64(GET_REG(instr), instr->constant, state);
        unmark_register(instr->base.reg, state);
    }
}

void __attribute__((always_inline)) emit_add(union InstructionIR* instruction, struct AssemblerState* state) {
    struct AddIR* instr = &instruction->ir_add;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

    Register64 a_register = GET_REG(instr_ensure(instr->a, state));
    Register64 b_register = GET_REG(instr_ensure(instr->b, state));
    bool a_assigned = IS_ASSIGNED(a_register);
    bool b_assigned = IS_ASSIGNED(b_register);

    if (a_assigned && b_assigned) {
        // we need to copy a into primary_reg, then add b into it
        asm_emit_add_r64_r64(this_reg, b_register, state);
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
        asm_emit_add_r64_r64(primary_reg, secondary_reg, state);
    }
}

void __attribute__((always_inline)) emit_sub(union InstructionIR* instruction, struct AssemblerState* state) {
    struct SubIR* instr = &instruction->ir_sub;

    if (!IS_ASSIGNED(GET_REG(instr))) return;
    Register64 this_reg = GET_REG(instr);
    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
    unmark_register(this_reg, state);

    Register64 a_register = GET_REG(instr_ensure(instr->a, state));
    Register64 b_register = GET_REG(instr_ensure(instr->b, state));
    bool a_assigned = IS_ASSIGNED(a_register);
    bool b_assigned = IS_ASSIGNED(b_register);

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


void __attribute__((always_inline)) emit_terminator(union TerminatorIR* terminator_ir, struct AssemblerState* state) {
    switch (terminator_ir->ir_base.id) {
        case ID_RETURN_IR: emit_return(terminator_ir, state); break;
        case ID_BRANCH_IR: emit_branch(terminator_ir, state); break;
        case ID_TERM_NONE:
        default:
            ojit_new_error();
            ojit_build_error_chars("Either Unimplemented or missing terminator: ID ");
            ojit_build_error_int(terminator_ir->ir_base.id);
            ojit_error();
            exit(-1); // TODO programmer error
    }
}

void __attribute__((always_inline)) emit_instruction(union InstructionIR* instruction_ir, struct AssemblerState* state) {
    switch (instruction_ir->base.id) {
        case ID_INT_IR: emit_int(instruction_ir, state); break;
        case ID_ADD_IR: emit_add(instruction_ir, state); break;
        case ID_SUB_IR: emit_sub(instruction_ir, state); break;
        case ID_PARAMETER_IR: break;
        default:
            ojit_new_error();
            ojit_build_error_chars("Broken or Unimplemented instruction: ");
            ojit_build_error_int(instruction_ir->base.id);
            ojit_error();
            exit(-1);
    }
}

struct BlockRecord {
    size_t offset;
    LAList* last_block;
};


struct CompiledFunction compile_function(CState* cstate, struct FunctionIR* func) {
    struct AssemblerState state;
    state.ctx = cstate->compiler_mem;

    size_t generated_size = 0;
    struct BlockRecord* block_records = ojit_alloc(cstate->compiler_mem, sizeof(struct BlockRecord) * func->num_blocks);
    LAListIter block_iter;
    lalist_init_iter(&block_iter, func->first_blocks, sizeof(struct BlockIR));
    struct BlockIR* block = lalist_iter_next(&block_iter);
    // start from the first block so entry is always first
    LAListIter instr_iter;
    union InstructionIR* instr;
    lalist_init_iter(&instr_iter, block->first_instrs, sizeof(union InstructionIR));
    instr = lalist_iter_next(&instr_iter);
    int param_num = 0;
    while (instr && instr->base.id == ID_PARAMETER_IR) {
        enum Register64 reg;
        switch (param_num) {
            case 0: reg = RCX; break;
            case 1: reg = RDX; break;
            case 2: reg = R8; break;
            case 3: reg = R9; break;
            default: exit(-1);  // TODO
        }
        instr_assign_reg(instr, AS_PARAM_REG(reg));
        param_num += 1;
        instr = lalist_iter_next(&instr_iter);
    }

    int i = 0;
    while (i < func->num_blocks) {
        LAList* init_mem = lalist_grow(cstate->compiler_mem, NULL, NULL);
        block_records[i].offset = generated_size;
        block_records[i].last_block = init_mem;
        init_asm_state(&state, init_mem);

        emit_terminator(&block->terminator, &state);
        generated_size += check_new_block(&state);


        lalist_init_iter(&instr_iter, block->last_instrs, sizeof(union InstructionIR));
        lalist_iter_position(&instr_iter, block->last_instrs->len - sizeof(union InstructionIR));
        instr = lalist_iter_prev(&instr_iter);
        int k = 0;
        while (instr) {
            emit_instruction(instr, &state);
            generated_size += check_new_block(&state);
            instr = lalist_iter_prev(&instr_iter);
            k += 1;
        }
        state.newest_block->len = LALIST_BLOCK_SIZE - (state.front_ptr - state.newest_block->mem);
        generated_size += state.newest_block->len;

        block = lalist_iter_next(&block_iter);
        i++;
    }
    uint8_t* func_mem = ojit_alloc(cstate->compiler_mem, generated_size);
    uint8_t* write_ptr = func_mem + generated_size;

    lalist_init_iter(&block_iter, func->first_blocks, sizeof(struct BlockIR));
    block = lalist_iter_prev(&block_iter);
    while (block) {
        struct BlockRecord record = block_records[block->block_num];
        LAList* curr_block = record.last_block;
        while (curr_block) {
            size_t block_size = curr_block->len;
            write_ptr -= block_size;
            ojit_memcpy(write_ptr, curr_block->mem + (LALIST_BLOCK_SIZE - block_size), block_size);

            curr_block = curr_block->prev;
        }
        block = lalist_iter_prev(&block_iter);
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

    // b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8c3
    // b8030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8b9030000004801c8c3

    return mem;
}
#else
#warning Executing jited code is currently unsupported on platforms other than Windows.
void* copy_to_executable(CState* bstate, void* from, size_t len) {
    ojit_fatal_error(bstate->error, "Executing jit'ed code is currently unsupported on platforms other than Windows.\n");
}
#endif
// endregion