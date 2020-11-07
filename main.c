#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>


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


// region Registers
// Idea: add Spilled-reg to mark values which were spilled onto the stack
// Idea: to simplify no-reg and spilled-reg, make it so values can't occupy RSP, RBP, R12, R13 and use those values to represent no and spilled
//       this would also simplify mov code, as r12 and r13 are special-cased, so if we can't normally use them, we don't have to write the case
//       this would also provide general purpose registers for uses like swapping variables and the like
enum Register64 {
    RAX = 0b0000,
    RCX = 0b0001,
    RDX = 0b0010,
    RBX = 0b0011,

    // We don't allow RSP and RBP to be used as registers
    NO_REG = 0b0100,
    SPILLED_REG = 0b0101,

    RSI = 0b0110,
    RDI = 0b0111,
    R8  = 0b1000,
    R9  = 0b1001,
    R10 = 0b1010,
    R11 = 0b1011,

    // We also don't allow R12 and R13 to be used as general purpose registers
    // Instead, reserve it for ourselves
    TMP_1_REG = 0b1100,
    TMP_2_REG = 0b1101,

    R14 = 0b1110,
    R15 = 0b1111,
};
// endregion


// region Instruction Base
enum InstructionID {
    ID_INSTR_NONE = 0,
    ID_INT_IR,
    ID_ADD_IR,
};

struct InstructionBase {
    enum InstructionID id;
    enum Register64 reg;
};
// endregion


// region Instructions
struct IntIR {
    struct InstructionBase base;
    int32_t constant;
};


struct AddIR {
    struct InstructionBase base;
    union InstructionIR* a;
    union InstructionIR* b;
};
// endregion


// region Instruction IR
union InstructionIR {
    struct InstructionBase base;
    struct IntIR ir_int;
    struct AddIR ir_add;
};

typedef union InstructionIR* IRValue;
// endregion


// region Instruction List
struct InstructionList {
    union InstructionIR* array;
    size_t cap;
    size_t len;
};


void init_instruction_list(struct InstructionList* list, size_t capacity) {
    list->array = malloc(sizeof(union InstructionIR) * capacity);
    if (list->array == NULL) {
        exit(-1);
    }
    list->cap = capacity;
    list->len = 0;
}


union InstructionIR* instruction_list_add_instruction(struct InstructionList* list) {
    if (list->len >= list->cap) {
        list->array = realloc(list->array, sizeof(union InstructionIR) * list->len * 2);
    }
    union InstructionIR* instr = &list->array[list->len++];

    return instr;
}
// endregion


// region Terminator Base
enum TerminatorID {
    ID_TERM_NONE = 0,
    ID_RETURN_IR,
    ID_BRANCH_IR,
};

struct TerminatorBase {
    enum TerminatorID id;
};
// endregion


// region Terminators
struct ReturnIR {
    struct TerminatorBase base;
    union InstructionIR* value;
};

struct BranchIR {
    struct TerminatorBase base;
    union TerminatorIR* target;
};
// endregion


// region Terminator IR
union TerminatorIR {
    struct TerminatorBase ir_base;
    struct ReturnIR ir_return;
    struct BranchIR ir_branch;
};
// endregion


// region Block
struct BlockIR {
    struct InstructionList instrs;
    union TerminatorIR terminator;
};


void init_block(struct BlockIR* block) {
    init_instruction_list(&block->instrs, 8);
    block->terminator.ir_base.id = ID_TERM_NONE;
}
// endregion


// region Block Builders
IRValue block_build_Int(struct BlockIR* block, int32_t constant) {
    struct IntIR* instr = &instruction_list_add_instruction(&block->instrs)->ir_int;
    instr->constant = constant;
    instr->base.id = ID_INT_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

IRValue block_build_Add(struct BlockIR* block, IRValue a, IRValue b) {
    struct AddIR* instr = &instruction_list_add_instruction(&block->instrs)->ir_add;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_ADD_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}


void block_terminate_Return(struct BlockIR* block, IRValue value) {
    struct ReturnIR* term = &block->terminator.ir_return;
    term->value = value;
    term->base.id = ID_RETURN_IR;
}
// endregion


// region BlockList
struct BlockList {
    struct BlockIR* array;
    size_t cap;
    size_t len;
};


void init_block_list(struct BlockList* list, size_t capacity) {
    list->array = malloc(sizeof(struct BlockIR) * capacity);
    list->cap = capacity;
    list->len = 0;
}


struct BlockIR* block_list_add_block(struct BlockList* list) {
    if (list->len >= list->cap) {
        list->cap = list->len*2;
        list->array = realloc(list->array, sizeof(struct BlockIR) * list->cap);
    }
    struct BlockIR* new_block = &list->array[list->len++];
    init_block(new_block);
    return new_block;
}
// endregion


// region Function
struct FunctionIR {
    struct BlockList blocks;
};

#define GET_BLOCK(func_ptr, n) (&(func_ptr)->blocks.array[(n)])


struct FunctionIR* create_function(char* name) {
    struct FunctionIR* function = malloc(sizeof(struct FunctionIR));
    init_block_list(&function->blocks, 8);

    block_list_add_block(&function->blocks);
    return function;
}


/* @entry (%maybe) {  //
 *      %a = 3;
 * } then cbranch(%maybe {0: @branch_zero, 1: @branch_one})(%a)   // e.a could be either RCX or RAX, so choose one and move when we branch
 * // also assign e.maybe to RCX since it's used by e.a could switch
 *
 *
 * @branch_zero(%b) {    // ...
 *      %a = 1;          // ...
 *      %c = %a - %b;    // b0.a: RAX; b1.b: RCX
 * } then return (%c);   // b0.c: RAX
 *
 *
 * @branch_one(%a) {     // ...
 *      %b = 2;          // ...
 *      %c = %a + %b;    // b1.a: RAX; b1.b: RCX
 * } then return (%c);   // b1.c: RAX
 *
 *
 *
 *
 * */

#define MAXIMUM_INSTRUCTION_SIZE (15)


#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))


enum Register64 get_unused(const bool* registers) {
    for (int i = 0; i < 16; i++) {
        if (!registers[i]) return i;
    }
    return NO_REG;
}


struct CompiledFunction {
    uint8_t * mem;
    size_t size;
};


struct CompiledFunction compile_function(struct FunctionIR* func) {
    void* func_mem = malloc(30);
    size_t cap = 30;
    size_t len = 0;

    for (size_t i = func->blocks.len-1; i < UINT32_MAX; i--) {
        struct BlockIR* block = &func->blocks.array[i];

        // I start writing from the end towards the front, then cutoff what I don't need
        size_t mem_size = (block->instrs.len + 1) * MAXIMUM_INSTRUCTION_SIZE;
        uint8_t* block_mem = malloc(mem_size);
        uint8_t* front_ptr = block_mem + mem_size;

        bool used_registers[16] = {
                [RAX] = false,
                [RCX] = false,
                [RDX] = false,
                [RBX] = false,
                [NO_REG] = true,
                [SPILLED_REG] = true,
                [RSI] = false,
                [RDI] = false,
                [R8 ] = false,
                [R9 ] = false,
                [R10] = false,
                [R11] = false,
                [TMP_1_REG] = true,
                [TMP_2_REG] = true,
                [R14] = false,
                [R15] = false,
        };

        switch (block->terminator.ir_base.id) {
            case ID_TERM_NONE:
                exit(-1);
            case ID_RETURN_IR: {
                struct ReturnIR* ret = &block->terminator.ir_return;

                assert(ret->value->base.reg == NO_REG);
                assert(!used_registers[RAX]);
                ret->value->base.reg = RAX;
                used_registers[RAX] = true;

                *(--front_ptr) = 0xC3;

                break;
            }
            case ID_BRANCH_IR: {
                exit(-1);
            }
        }

        for (size_t j = block->instrs.len-1; j < UINT32_MAX; j--) {
            switch (block->instrs.array[j].base.id) {
                case ID_INT_IR: {
                    struct IntIR* instr = &block->instrs.array[j].ir_int;
                    uint32_t constant = instr->constant;
                    if (instr->base.reg == NO_REG) {
                        // then this wasn't used later, so just skip
                    } else {
                        *(--front_ptr) = (constant >> 24) & 0xFF;
                        *(--front_ptr) = (constant >> 16) & 0xFF;
                        *(--front_ptr) = (constant >>  8) & 0xFF;
                        *(--front_ptr) = (constant >>  0) & 0xFF;
                        *(--front_ptr) = MODRM(0b11, 0b000, instr->base.reg & 0b0111);
                        *(--front_ptr) = 0xC7;
                        *(--front_ptr) = REX(0b1, 0b0, 0b0, instr->base.reg >> 3 & 0b0001);

                        used_registers[instr->base.reg] = false;
                    }
                    break;
                }
                case ID_ADD_IR: {
                    struct AddIR* instr = &block->instrs.array[j].ir_add;
                    if (instr->base.reg == NO_REG) {
                        // then this wasn't used later, so just skip
                    } else {
                        // wow, thinking backwards in time is hard
                        bool reg_reused = false;
                        if (instr->a->base.reg == NO_REG) {
                            instr->a->base.reg = instr->base.reg;
                            reg_reused = true;
                        }
                        if (instr->b->base.reg == NO_REG) {
                            if (reg_reused) {
                                enum Register64 reg = get_unused(used_registers);
                                if (reg == NO_REG) {
                                    // TODO register spilling
                                    // due to the one-pass strategy, only this register is a viable target to spill
                                    exit(-1);
                                } else {
                                    instr->b->base.reg = reg;
                                }
                            } else {
                                instr->b->base.reg = instr->base.reg;
                                reg_reused = true;
                            }
                        }

                        enum Register64 a_reg = instr->a->base.reg;
                        enum Register64 b_reg = instr->b->base.reg;
                        if (reg_reused) {
                            // yay, just add into that register
                            if (a_reg == instr->base.reg) {
                                *(--front_ptr) = MODRM(0b11, instr->base.reg & 0b0111, b_reg & 0b111);
                                *(--front_ptr) = 0x03;
                                *(--front_ptr) = REX(0b1, instr->base.reg >> 3 & 0b1, 0b0, b_reg >> 3 & 0b1);
                            } else {
                                *(--front_ptr) = MODRM(0b11, instr->base.reg & 0b0111, a_reg & 0b111);
                                *(--front_ptr) = 0x03;
                                *(--front_ptr) = REX(0b1, instr->base.reg >> 3 & 0b1, 0b0, a_reg >> 3 & 0b1);
                            }
                        } else {
                            // oh no. we have to move a to add, then add b to it.
                            // which means we write an add, then a move, because weirdness
                            *(--front_ptr) = MODRM(0b11, instr->base.reg & 0b0111, b_reg & 0b111);
                            *(--front_ptr) = 0x03;
                            *(--front_ptr) = REX(0b1, instr->base.reg >> 3 & 0b1, 0b0, b_reg >> 3 & 0b1);

                            *(--front_ptr) = MODRM(0b11, a_reg & 0b111, instr->base.reg & 0b0111);
                            *(--front_ptr) = 0x89;
                            *(--front_ptr) = REX(0b1, a_reg >> 3 & 0b1, 0b0, instr->base.reg >> 3 & 0b1);
                        }
                        used_registers[instr->base.reg] = false;
                    }
                    break;
                }
                case ID_INSTR_NONE: {
                    exit(-1);
                }
            }
        }

        size_t block_size = mem_size - (front_ptr - block_mem);
        if (len + block_size > cap) {
            func_mem = realloc(func_mem, (len + block_size) * 2);
        }
        memmove(func_mem + len, front_ptr, block_size);
        len += block_size;
        free(block_mem);
    }

    func_mem = realloc(func_mem, len);

    return (struct CompiledFunction) {func_mem, len};
}


// endregion


typedef int (*FuncType)();


int main() {
    struct FunctionIR* function = create_function("main");
    struct BlockIR* entry = GET_BLOCK(function, 0);

    IRValue const_1 = block_build_Int(entry, 1);
    IRValue const_2 = block_build_Int(entry, 2);
    IRValue added   = block_build_Add(entry, const_1, const_2);
    block_terminate_Return(entry, added);

    struct CompiledFunction compiled = compile_function(function);
    for (int i = 0; i < compiled.size; i++ ) {
        printf("%02x", compiled.mem[i]);
    }
    printf("\n");

    FuncType func = (FuncType) copy_to_executable(compiled.mem, compiled.size);
    printf("Value: %i\n", func());

    return 0;
}
