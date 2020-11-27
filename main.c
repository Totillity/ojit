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

    // PARAM_regs serve to help us mark registers
    // We can convert by adding and subtracting 16 (0x10000)
    PARAM_RAX = 0b10000,
    PARAM_RCX = 0b10001,
    PARAM_RDX = 0b10010,
    PARAM_RBX = 0b10011,

    PARAM_NO_REG = 0b10100,
    PARAM_SPILLED_REG = 0b10101,

    PARAM_RSI = 0b10110,
    PARAM_RDI = 0b10111,
    PARAM_R8  = 0b11000,
    PARAM_R9  = 0b11001,
    PARAM_R10 = 0b11010,
    PARAM_R11 = 0b11011,

    PARAM_TMP_1_REG = 0b11100,
    PARAM_TMP_2_REG = 0b11101,

    PARAM_R14 = 0b11110,
    PARAM_R15 = 0b11111,
};
typedef enum Register64 Register64;

#define NORMALIZE_REG(reg) ((reg) & 0b1111)
#define IS_PARAM_REG(reg) (((reg) & 0b10000) != 0)
#define AS_PARAM_REG(reg) ((reg) | 0b10000)
#define IS_ASSIGNED(reg) (((reg) & 0b1111) != NO_REG)
// endregion


// region Instruction Base
enum InstructionID {
    ID_INSTR_NONE = 0,
    ID_PARAMETER_IR,
    ID_INT_IR,
    ID_ADD_IR,
};

struct InstructionBase {
    enum InstructionID id;
    Register64 reg;
};
// endregion


// region Instructions
struct ParameterIR {
    struct InstructionBase base;
};


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
    struct ParameterIR ir_parameter;
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


// region IRValueList
#define VALUE_LIST_INIT_SIZE (4)

struct IRValueList {
    union InstructionIR** array;
    size_t cap;
    size_t len;
};

void init_value_list(struct IRValueList* list, size_t item_count, IRValue* items) {
    if (item_count) {
        list->array = malloc(sizeof(union InstructionIR*) * item_count);
        if (list->array == NULL) {
            exit(-1);
        }
        memcpy(list->array, items, sizeof(union InstructionIR*) * item_count);
        list->cap = item_count;
        list->len = item_count;
    } else {
        list->array = malloc(sizeof(union InstructionIR*) * VALUE_LIST_INIT_SIZE);
        if (list->array == NULL) {
            exit(-1);
        }
        memcpy(list->array, items, sizeof(union InstructionIR*) * VALUE_LIST_INIT_SIZE);
        list->cap = VALUE_LIST_INIT_SIZE;
        list->len = 0;
    }

}


void value_list_add_value(struct IRValueList* list, IRValue value) {
    if (list->len >= list->cap) {
        list->array = realloc(list->array, sizeof(union InstructionIR) * list->len * 2);
    }
    list->array[list->len++] = value;
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
    struct BlockIR* target;
    struct IRValueList arguments;

    size_t offset_from_end;
    struct BranchIR* next_listener;
    struct BlockIR* in_block;
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

    uint8_t* code_ptr;
    struct BranchIR* next_listener;
};


void init_block(struct BlockIR* block) {
    init_instruction_list(&block->instrs, 8);
    block->terminator.ir_base.id = ID_TERM_NONE;

    block->code_ptr = NULL;
    block->next_listener = NULL;
}
// endregion


// region Block Builders
IRValue block_add_parameter(struct BlockIR* block) {
    struct ParameterIR* instr = &instruction_list_add_instruction(&block->instrs)->ir_parameter;
    instr->base.id = ID_PARAMETER_IR;
    instr->base.reg = NO_REG;
    return (IRValue) instr;
}


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

void block_terminate_Branch(struct BlockIR* block, struct BlockIR* target, size_t arg_count, IRValue* arguments) {
    struct BranchIR* term = &block->terminator.ir_branch;
    term->target = target;
    init_value_list(&term->arguments, arg_count, arguments);
    term->offset_from_end = 0;
    term->next_listener = NULL;
    term->in_block = block;
    term->base.id = ID_BRANCH_IR;
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


struct BlockIR* function_add_block(struct FunctionIR* func) {
    return block_list_add_block(&func->blocks);
}

// endregion Function


// region Compilation
#define MAXIMUM_INSTRUCTION_SIZE (15)

// region Utility Functions & Macros
#define REX(w, r, x, b) ((uint8_t) (0b01000000 | ((w) << 3) | ((r) << 2) | ((x) << 1) | ((b))))
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))


#define MARK_REG(num)  do {assert(!used_registers[(num)]); used_registers[(num)] = true;} while (0)
#define UNMARK_REG(num)  do {assert(used_registers[(num)]); used_registers[(num)] = false;} while (0)
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
        uint8_t* end_ptr = block_mem + mem_size;

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
                                // TODO register spilling
                                exit(-1);
                            } else {
                                ASSIGN_REG(argument->base.reg, reg);
                                MARK_REG(argument->base.reg);
                                // We need to assign the PARAM version so the block knows to mark it
                                ASSIGN_REG(param->base.reg, AS_PARAM_REG(reg));
                            }
                        }
                    } else {
                        printf("Too many arguments");
                        exit(-1);
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
                exit(-1);
            }
            default: {
                printf("Unimplemented terminator");
                exit(-1);
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

                    if (IS_ASSIGNED(instr->base.reg)) {
                        UNMARK_REG(instr->base.reg);
                    }
                    Register64 a_register = PARAM_CHECK(instr->a->base.reg);
                    Register64 b_register = PARAM_CHECK(instr->b->base.reg);
                    if (IS_ASSIGNED(instr->base.reg)) {
                        bool a_assigned = IS_ASSIGNED(a_register);
                        bool b_assigned = IS_ASSIGNED(b_register);
                        Register64 primary_reg;  // the register we add into to get the result
                        Register64 secondary_reg; // the register we add in

                        if (a_assigned && b_assigned) {
                            // so just a mov to get stuff in the right place?
                            primary_reg = instr->base.reg;
                            secondary_reg = b_register;
                        } else if (a_assigned) {
                            // use b as our primary register which is added into and contains the result
                            // after all, this is b's last (or first) use, so it's safe
                            ASSIGN_REG(instr->b->base.reg, instr->base.reg);
                            primary_reg = b_register;
                            secondary_reg = a_register;
                        } else if (b_assigned) {
                            // use a as our primary register which is added into and contains the result
                            // after all, this is a's last (or first) use, so it's safe
                            ASSIGN_REG(instr->a->base.reg, instr->base.reg);
                            primary_reg = a_register;
                            secondary_reg = b_register;
                        } else {
                            // use a as our primary register which is added into and contains the result
                            // after all, this is a's last (or first) use, so it's safe
                            ASSIGN_REG(instr->a->base.reg, instr->base.reg);
                            primary_reg = a_register;
                            secondary_reg = b_register;

                            Register64 reg = get_unused(used_registers);
                            if (reg == NO_REG) {
                                exit(-1); // TODO spill
                            } else {
                                ASSIGN_REG(instr->b->base.reg, reg);
                                MARK_REG(reg);
                            }
                        }

                        EMIT_ADD_R64_R64(primary_reg, secondary_reg);
                        if (primary_reg == instr->base.reg) {
                            // we have to move 'a' into it first
                            EMIT_MOV_R64_R64(primary_reg, a_register);
                        }
                    }
                    break;
                }
                case ID_PARAMETER_IR: {
                    break;
                }

                case ID_INSTR_NONE: {
                    exit(-1);
                }
                default: {
                    printf("Unimplemented instruction");
                    exit(-1);
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
    struct FunctionIR* function = create_function("main");

    struct BlockIR* entry = GET_BLOCK(function, 0);

    struct BlockIR* end   = function_add_block(function);
    IRValue param_1 = block_add_parameter(end);
    IRValue param_2 = block_add_parameter(end);

    IRValue const_1 = block_build_Int(entry, 1);
    IRValue const_2 = block_build_Int(entry, 2);
    IRValue arguments[2] = {const_1, const_2};
    block_terminate_Branch(entry, end, 2, arguments);

    IRValue added   = block_build_Add(end, param_1, param_2);
    block_terminate_Return(end, added);

    struct CompiledFunction compiled = compile_function(function);
    for (int i = 0; i < compiled.size; i++ ) {
        printf("%02x", compiled.mem[i]);
    }
    printf("\n");

    FuncType func = (FuncType) copy_to_executable(compiled.mem, compiled.size);
    printf("Value: %i\n", func());

    return 0;
}
