#include <assert.h>
#include <stdio.h>

#include "compiler.h"

// region Memory
#include <windows.h>
void* copy_to_executable(void* from, size_t len) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    void* mem = VirtualAlloc(NULL, info.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    memcpy(mem, from, len);

    DWORD dummy;
    VirtualProtect(mem, len, PAGE_EXECUTE_READ, &dummy);

    return mem;
}
// endregion


// region Compilation
#define MAXIMUM_INSTRUCTION_SIZE (15)

// region Utility Functions & Macros
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

Register64 get_unused(const bool* registers) {
    for (int i = 0; i < 16; i++) {
        if (!registers[i]) return i;
    }
    return NO_REG;
}
// endregion


struct MemBlock {
    uint8_t* front_ptr;
    struct MemBlock* next_block;
    uint8_t mem[256];
};


struct AssemblerState {
    uint8_t* front_ptr;
    bool used_registers[16];
    struct MemBlock* block;
};


struct MemBlock* add_mem_block(struct MemBlock* last_block) {
    struct MemBlock* block = malloc(sizeof(struct MemBlock));
    if (last_block) last_block->next_block = block;
    block->next_block = NULL;
    block->front_ptr = NULL;
    memset(block->mem, 0, 256);
    return block;
}


void init_asm_state(struct AssemblerState* state, struct MemBlock* block) {
    state->front_ptr = &block->mem[256];

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

    state->block = block;
}


#define GET_REG(value) ((value)->base.reg)
#define SET_REG(value, regi) ((value)->base.reg = (regi))
#define REG_IS_MARKED(reg) (state->used_registers[(reg)])


// region asm utility
void __attribute__((always_inline)) asm_emit_byte(uint8_t byte, struct AssemblerState* state) {
    *(--state->front_ptr) = byte;
}

void __attribute__((always_inline)) asm_emit_int32(uint32_t constant, struct AssemblerState* state) {
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

void __attribute__((always_inline)) asm_emit_mov_r64_i32(Register64 dest, uint32_t constant, struct AssemblerState* state) {
    asm_emit_int32(constant, state);
    asm_emit_byte(MODRM(0b11, 0b000, dest & 0b0111), state);
    asm_emit_byte(0xC7, state);
    asm_emit_byte(REX(0b1, 0b0, 0b0, dest >> 3 & 0b0001), state);
}

void __attribute__((always_inline)) asm_emit_add_r64_r64(Register64 dest, Register64 source, struct AssemblerState* state) {
    asm_emit_byte(MODRM(0b11, source & 0b111, dest & 0b0111), state);
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
//    bool* registers_state = state.used_registers;
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

size_t __attribute__((always_inline)) get_block_size(struct MemBlock* block) {
    return 256 - (block->front_ptr - block->mem);
}

size_t __attribute__((always_inline)) check_block(struct AssemblerState* state) {
    if (state->front_ptr - state->block->mem > 16) {
        state->block->front_ptr = state->front_ptr;
        size_t generated_amount = get_block_size(state->block);
        state->block = add_mem_block(state->block);
        state->front_ptr = &state->block->mem[256];
        return generated_amount;
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

    for (int k = 0; k < branch->arguments.len; k++) {
        IRValue argument = branch->arguments.array[k];

        struct ParameterIR* param = (struct ParameterIR*) &branch->target->instrs.array[k];
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
        asm_emit_mov_r64_i32(GET_REG(instr), instr->constant, state);
        unmark_register(instr->base.reg, state);
    }
}


//void __attribute__((always_inline)) bin_op_registers() {
//    bool a_assigned = IS_ASSIGNED(a_register);
//    bool b_assigned = IS_ASSIGNED(b_register);
//
//    if (a_assigned && b_assigned) {
//        // we need to copy a into primary_reg, then add b into it
//        asm_emit_add_r64_r64(this_reg, b_register, state);
//        asm_emit_mov_r64_r64(this_reg, a_register, state);
//    } else {
//        Register64 primary_reg;  // the register we add into to get the result
//        Register64 secondary_reg; // the register we add in
//
//        if (a_assigned) {
//            // use b as our primary register which is added into and contains the result
//            // after all, this is b's last (or first) use, so it's safe
//            instr_assign_reg(instr->b, this_reg);
//            mark_register(this_reg, state);
//            primary_reg = this_reg;
//            secondary_reg = a_register;
//        } else if (b_assigned) {
//            // use a as our primary register which is added into and contains the result
//            // after all, this is a's last (or first) use, so it's safe
//            instr_assign_reg(instr->a, this_reg);
//            mark_register(this_reg, state);
//            primary_reg = this_reg;
//            secondary_reg = b_register;
//        } else {
//            // use a as our primary register which is added into and contains the result
//            // after all, this is a's last (or first) use, so it's safe
//            instr_assign_reg(instr->a, this_reg);
//            mark_register(this_reg, state);
//
//            // now find a register to put b into
//            Register64 new_reg = get_unused(state->used_registers);
//            if (new_reg == NO_REG) {
//                printf("Too many registers used concurrently");
//                exit(-1); // TODO spill
//            }
//            instr_assign_reg(instr->b, new_reg);
//            mark_register(new_reg, state);
//
//            primary_reg = this_reg;
//            secondary_reg = new_reg;
//        }
//        asm_emit_add_r64_r64(primary_reg, secondary_reg, state);
//    }
//}

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
            printf("Either Unimplemented or missing terminator");
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
            printf("Broken or Unimplemented instruction");
            exit(-1);  // TODO programmer error
    }
}

struct BlockRecord {
    size_t offset;
    struct MemBlock* end_mem;
};


struct CompiledFunction compile_function(struct FunctionIR* func) {
    struct AssemblerState state;

    size_t generated_size = 0;
    struct BlockRecord* block_records = calloc(func->blocks.len, sizeof(struct BlockRecord));
    // start from the first block so entry is always first
    for (size_t i = 0; i < func->blocks.len; i++) {
        struct BlockIR* block = &func->blocks.array[i];
        struct MemBlock* block_mem_block = add_mem_block(NULL);
        block_records[i].offset = generated_size;
        block_records[i].end_mem = block_mem_block;
        init_asm_state(&state, block_mem_block);

        emit_terminator(&block->terminator, &state);
        generated_size += check_block(&state);
        for (size_t j = block->instrs.len; j > 0; j--) {
            emit_instruction(&block->instrs.array[j-1], &state);
            generated_size += check_block(&state);
        }
        state.block->front_ptr = state.front_ptr;
        generated_size += get_block_size(state.block);
    }

    uint8_t* func_mem = malloc(generated_size);
    uint8_t* write_ptr = func_mem + generated_size;
    for (size_t i = func->blocks.len; i > 0; i--) {
        struct BlockRecord record = block_records[i-1];
        struct MemBlock* curr_block = record.end_mem;
        while (curr_block) {
            size_t block_size = get_block_size(curr_block);
            write_ptr -= block_size;
            memcpy(write_ptr, curr_block->front_ptr, block_size);

            curr_block = curr_block->next_block;
        }
    }

    return (struct CompiledFunction) {func_mem, generated_size};
}
// endregion