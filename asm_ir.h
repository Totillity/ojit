#ifndef OJIT_ASM_IR_H
#define OJIT_ASM_IR_H

#include "ojit_def.h"
#include "ojit_mem.h"

#include "string_tools/ojit_string.h"
#include "hash_table/hash_table.h"

// region Registers
// Idea: add Spilled-reg to mark values which were spilled onto the stack
// Idea: to simplify no-reg and spilled-reg, make it so values can't occupy RSP, RBP, R12, R13 and use those values to represent no and spilled
//       this would also_lvalue simplify mov code, as r12 and r13 are special-cased, so if we can't normally use them, we don't have to write the case
//       this would also_lvalue provide general purpose registers for uses like swapping variables and the like
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

    // We also_lvalue don't allow R12 and R13 to be used as general purpose registers
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

// region Instruction
typedef union u_InstructionIR Instruction;

enum InstructionID {
    ID_INSTR_NONE = 0,
    ID_PARAMETER_IR,
    ID_INT_IR,
    ID_ADD_IR,
    ID_SUB_IR,
    ID_CALL_IR,
    ID_GLOBAL_IR,
};

struct InstructionBase {
    Register64 reg;
    enum InstructionID id;
};

struct ParameterIR {
    struct InstructionBase base;
};

struct IntIR {
    struct InstructionBase base;
    int32_t constant;
};

struct AddIR {
    struct InstructionBase base;
    Instruction* a;
    Instruction* b;
};

struct SubIR {
    struct InstructionBase base;
    Instruction* a;
    Instruction* b;
};

struct CallIR {
    struct InstructionBase base;
    Instruction* callee;
    LAList* arguments;
};

struct GlobalIR {
    struct InstructionBase base;
    String name;
};

union u_InstructionIR {
    struct InstructionBase base;
    struct ParameterIR ir_parameter;
    struct IntIR ir_int;
    struct AddIR ir_add;
    struct SubIR ir_sub;
    struct CallIR ir_call;
    struct GlobalIR ir_global;
};

typedef Instruction* IRValue;
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

struct ReturnIR {
    struct TerminatorBase base;
    Instruction* value;
};

struct BranchIR {
    struct TerminatorBase base;
    struct BlockIR* target;
    IRValue* arguments;
    size_t argument_count;
};

union TerminatorIR {
    struct TerminatorBase ir_base;
    struct ReturnIR ir_return;
    struct BranchIR ir_branch;
};
// endregion

// region Block
struct BlockIR {
    LAList* first_instrs;
    LAList* last_instrs;
    union TerminatorIR terminator;
    struct HashTable variables;
    size_t block_num;
};

void init_block(struct BlockIR* block, size_t block_num, MemCtx* ctx);
// endregion

// region Function
struct FunctionIR {
    String name;
    LAList* first_blocks;
    LAList* last_blocks;
    size_t num_blocks;

    void* compiled;
};

struct FunctionIR* create_function(String name, MemCtx* ctx);
struct BlockIR* function_add_block(struct FunctionIR* func, MemCtx* ctx);
// endregion Function


#endif //OJIT_ASM_IR_H
