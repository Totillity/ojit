#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

//#include "parser/parser.h"
#include "asm_ir_builders.h"
#include "program_ir.h"

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


#define MARK_REG(num)  do {assert(used_registers[(num)] == false); used_registers[(num)] = true;} while (0)
#define UNMARK_REG(num)  do {assert(used_registers[(num)] == true); used_registers[(num)] = false;} while (0)
#define ASSIGN_REG(value, reg) do {assert(!IS_ASSIGNED((value))); (value) = (reg);} while (0)

#define PARAM_CHECK(reg) ({ if (IS_PARAM_REG(reg)) { (reg) = NORMALIZE_REG(reg); MARK_REG(reg); } (reg);})

#define EMIT_MOV_R64_R64(to_reg, from_reg) \
    do {*(--front_ptr) = MODRM(0b11, (from_reg) & 0b111, (to_reg) & 0b0111); \
        *(--front_ptr) = 0x89; \
        *(--front_ptr) = REX(0b1, (from_reg) >> 3 & 0b1, 0b0, (to_reg) >> 3 & 0b1);} while (0)
#define EMIT_ADD_R64_R64(source_reg, to_reg) \
    do {*(--front_ptr) = MODRM(0b11, (to_reg) & 0b111, (source_reg) & 0b0111); \
        *(--front_ptr) = 0x01; \
        *(--front_ptr) = REX(0b1, (to_reg) >> 3 & 0b1, 0b0, (source_reg) >> 3 & 0b1);} while (0)
#define EMIT_INTEGER32(constant) \
    do {*(--front_ptr) = ((constant) >> 24) & 0xFF; \
        *(--front_ptr) = ((constant) >> 16) & 0xFF; \
        *(--front_ptr) = ((constant) >>  8) & 0xFF; \
        *(--front_ptr) = ((constant) >>  0) & 0xFF;} while (0)

Register64 get_unused(const bool* registers) {
    for (int i = 0; i < 16; i++) {
        if (!registers[i]) return i;
    }
    return NO_REG;
}
// endregion

struct CompiledFunction {
    uint8_t * mem;
    size_t size;
};


struct CompiledFunction compile_function(struct FunctionIR* func) {
    uint8_t* func_mem = malloc(30);
    size_t cap = 30;
    size_t len = 0;

    // start from the first block so entry is always first
    for (size_t i = 0; i < func->blocks.len; i++) {
        struct BlockIR* block = &func->blocks.array[i];

        // I start writing from the end towards the front, then cutoff what I don't need
        size_t mem_size = (block->instrs.len + 1) * MAXIMUM_INSTRUCTION_SIZE;
        uint8_t* block_mem = malloc(mem_size);
        memset(block_mem, 0, mem_size);
        uint8_t* front_ptr = block_mem + mem_size;
//        uint8_t* end_ptr = block_mem + mem_size;

        bool used_registers[16] = {
                [RAX] = false,
                [RCX] = false,
                [RDX] = false,
                [RBX] = false,
                [NO_REG] = true,
                [SPILLED_REG] = true,
                [RSI] = false,
                [RDI] = false,
                [R8]  = false,
                [R9]  = false,
                [R10] = false,
                [R11] = false,
                [TMP_1_REG] = true,
                [TMP_2_REG] = true,
                [R14] = false,
                [R15] = false,
        };

        switch (block->terminator.ir_base.id) {
            case ID_RETURN_IR: {
                struct ReturnIR* ret = &block->terminator.ir_return;

                // BTW, PARAM_NO_REG shouldn't be possible to ever make
                if (IS_ASSIGNED(ret->value->base.reg)) {
                    // if this just returns a parameter (which could already be assigned), then move the parameter to RAX to return it
                    Register64 value_reg = PARAM_CHECK(ret->value->base.reg);

                    *(--front_ptr) = 0xC3;
                    EMIT_MOV_R64_R64(RAX, value_reg);
                } else {
                    ASSIGN_REG(ret->value->base.reg, RAX);
                    MARK_REG(RAX);

                    *(--front_ptr) = 0xC3;
                }
                break;
            }
            case ID_BRANCH_IR: {
                struct BranchIR* branch = &block->terminator.ir_branch;

                for (int k = 0; k < branch->arguments.len; k++) {
                    IRValue argument = branch->arguments.array[k];
                    union InstructionIR* instr = &branch->target->instrs.array[k];
                    if (instr->base.id == ID_PARAMETER_IR) {
                        struct ParameterIR* param = &instr->ir_parameter;
                        
                        Register64 param_reg = PARAM_CHECK(param->base.reg);
                        Register64 argument_reg = PARAM_CHECK(argument->base.reg);

                        bool param_assigned = IS_ASSIGNED(param_reg);
                        bool arg_assigned = IS_ASSIGNED(argument_reg);
                        if (param_assigned && arg_assigned) {
                            EMIT_MOV_R64_R64(param_reg, argument_reg);
                        } else if (param_assigned) {
                            ASSIGN_REG(argument->base.reg, param_reg);
                            MARK_REG(argument->base.reg);
                        } else if (arg_assigned) {
                            // so say the parameter's reg is the argument's reg
                            ASSIGN_REG(param->base.reg, AS_PARAM_REG(argument_reg));
                        } else {
                            // oh god they're both unassigned
                            // find an unused register and assign it to both
                            Register64 reg = get_unused(used_registers);
                            if (reg == NO_REG) {
                                printf("Too many registers used concurrently");
                                exit(-1);  // TODO register spilling
                            } else {
                                ASSIGN_REG(argument->base.reg, reg);
                                MARK_REG(argument->base.reg);
                                // We need to assign the PARAM version so the block knows to mark it
                                ASSIGN_REG(param->base.reg, AS_PARAM_REG(reg));
                            }
                        }
                    } else {
                        printf("Too many arguments");
                        exit(-1);  // TODO programmer error
                    }
                }

                if (branch->target->code_ptr) {
                    ssize_t offset = branch->target->code_ptr - front_ptr;
                    EMIT_INTEGER32((size_t) offset);
                    *(--front_ptr) = 0xE9;
                } else {
                    branch->next_listener = branch->target->next_listener;
                    branch->target->next_listener = branch;
                    branch->offset_from_end = mem_size - (front_ptr - block_mem);
                    EMIT_INTEGER32(0xEFBEADDE);
                    *(--front_ptr) = 0xE9;
                }
                break;
            }
            case ID_TERM_NONE: {
                printf("No terminator");
                exit(-1);  // TODO programmer error
            }
            default: {
                printf("Unimplemented terminator");
                exit(-1); // TODO programmer error
            }
        }

        for (size_t j = block->instrs.len-1; j < UINT32_MAX; j--) {
            enum InstructionID id = block->instrs.array[j].base.id;
            switch (id) {
                case ID_INT_IR: {
                    struct IntIR* instr = &block->instrs.array[j].ir_int;
                    if (instr->base.reg == NO_REG) {
                        // then this wasn't used later, so just skip
                    } else {
                        EMIT_INTEGER32(instr->constant);
                        *(--front_ptr) = MODRM(0b11, 0b000, instr->base.reg & 0b0111);
                        *(--front_ptr) = 0xC7;
                        *(--front_ptr) = REX(0b1, 0b0, 0b0, instr->base.reg >> 3 & 0b0001);

                        UNMARK_REG(instr->base.reg);
                    }
                    break;
                }
                case ID_ADD_IR: {
                    struct AddIR* instr = &block->instrs.array[j].ir_add;

                    if (!IS_ASSIGNED(instr->base.reg)) break;

                    // by unmarking the register the result is stored in, we can use it as the register of one of the arguments
                    Register64 this_reg = instr->base.reg;
                    if (IS_ASSIGNED(this_reg)) {
                        UNMARK_REG(this_reg);
                    }

                    Register64 a_register = PARAM_CHECK(instr->a->base.reg);
                    Register64 b_register = PARAM_CHECK(instr->b->base.reg);
                    bool a_assigned = IS_ASSIGNED(a_register);
                    bool b_assigned = IS_ASSIGNED(b_register);

                    if (a_assigned && b_assigned) {
                        // we need to copy a into primary_reg, then add b into it
                        EMIT_ADD_R64_R64(this_reg, b_register);
                        EMIT_MOV_R64_R64(this_reg, a_register);
                    } else {
                        Register64 primary_reg;  // the register we add into to get the result
                        Register64 secondary_reg; // the register we add in

                        if (a_assigned) {
                            // use b as our primary register which is added into and contains the result
                            // after all, this is b's last (or first) use, so it's safe
                            ASSIGN_REG(instr->b->base.reg, this_reg);
                            MARK_REG(this_reg);
                            primary_reg = this_reg;
                            secondary_reg = a_register;
                        } else if (b_assigned) {
                            // use a as our primary register which is added into and contains the result
                            // after all, this is a's last (or first) use, so it's safe
                            ASSIGN_REG(instr->a->base.reg, this_reg);
                            MARK_REG(this_reg);
                            primary_reg = this_reg;
                            secondary_reg = b_register;
                        } else {
                            // use a as our primary register which is added into and contains the result
                            // after all, this is a's last (or first) use, so it's safe
                            ASSIGN_REG(instr->a->base.reg, this_reg);
                            MARK_REG(this_reg);

                            // now find a register to put b into
                            Register64 reg_1 = get_unused(used_registers);
                            if (reg_1 == NO_REG) {
                                printf("Too many registers used concurrently");
                                exit(-1); // TODO spill
                            } else {
                                ASSIGN_REG(instr->b->base.reg, reg_1);
                                MARK_REG(reg_1);
                            }
                            primary_reg = this_reg;
                            secondary_reg = reg_1;
                        }
                        EMIT_ADD_R64_R64(primary_reg, secondary_reg);
                    }
                    break;
                }
                case ID_PARAMETER_IR: {
                    break;
                }

                case ID_INSTR_NONE: {
                    printf("Broken instruction");
                    exit(-1);  // TODO programmer error
                }
                default: {
                    printf("Unimplemented instruction");
                    exit(-1);  // TODO programmer error
                }
            }
        }

        // find the actual size of the block's code (which is a bit harder because we do it backwards)
        size_t block_size = mem_size - (front_ptr - block_mem);

        // stick that memory onto the end of the function's code
        if (len + block_size > cap) {
            func_mem = realloc(func_mem, (len + block_size) * 2);
        }

        memmove(func_mem + len, front_ptr, block_size);
        block->code_ptr = func_mem + len;
        struct BranchIR* listener = block->next_listener;
        while (listener) {
            struct BlockIR* next_block = &listener->in_block[1];
            uint8_t* constant_ptr  = next_block->code_ptr - listener->offset_from_end;
            ssize_t offset = func_mem + len - constant_ptr;
            front_ptr = constant_ptr;
            EMIT_INTEGER32((uint32_t) offset);
            listener = listener->next_listener;
        }
        len += block_size;

        // and free what we stored the block's code in
        free(block_mem);
    }

    func_mem = realloc(func_mem, len);

    return (struct CompiledFunction) {func_mem, len};
}
// endregion


typedef int (*FuncType)();


int main() {
    ProgramIR program_ir;
    create_program_ir(&program_ir);

    String source = program_ir_read_file(&program_ir, "test.txt");
    program_ir_parse_source(&program_ir, source);
    struct FunctionIR* main;
    program_ir_get_function_r(&program_ir, "main", 4, &main);


//    // region Construct IR
//    struct FunctionIR* function = create_function(STRING("main"));
//
//    struct BlockIR* entry = GET_BLOCK(function, 0);
//
//    struct BlockIR* end   = function_add_block(function);
//    IRValue param_1 = block_add_parameter(end);
//    IRValue param_2 = block_add_parameter(end);
//
//    IRValue const_1 = block_build_Int(entry, 1);
//    IRValue const_2 = block_build_Int(entry, 2);
//    IRValue arguments[2] = {const_1, const_2};
//    block_terminate_Branch(entry, end, 2, arguments);
//
//    IRValue added   = block_build_Add(end, param_1, param_2);
//    block_terminate_Return(end, added);
//    // endregion

    struct CompiledFunction compiled = compile_function(main);
    for (int i = 0; i < compiled.size; i++ ) {
        printf("%02x", compiled.mem[i]);
    }
    printf("\n");

    FuncType func = (FuncType) copy_to_executable(compiled.mem, compiled.size);
    printf("Value: %i\n", func());

    return 0;
}
