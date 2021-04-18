#ifndef OJIT_ASM_IR_H
#define OJIT_ASM_IR_H

#include "ojit_def.h"
#include "ojit_mem.h"

#include "ojit_string.h"
#include "hash_table.h"

#define FOREACH_INSTR(iter_var, iter_over) LAListIter iter_##iter_var; \
                                           lalist_init_iter(&iter_##iter_var, (iter_over), 0, sizeof(Instruction)); \
                                           Instruction* iter_var; \
                                           while (((iter_var) = lalist_iter_next(&iter_##iter_var)) != NULL)

#define INC_INSTR(instr) ((instr)->base.refs++)
#define DEC_INSTR(instr) ((instr)->base.refs--)
#define INSTR_REF(instr) ((instr)->base.refs)

#define INSTR_TYPE(val) ((val)->base.id)

// region Registers
// Idea: add Spilled-reg to mark values which were spilled onto the stack
enum Registers {
    RAX = 0b0000,
    RCX = 0b0001,
    RDX = 0b0010,
    RBX = 0b0011,

    // We don't allow RSP and RBP to be used as registers
    RSP = 0b0100, NO_REG = 0b0100,
    RBP = 0b0101, SPILLED_REG = 0b0101,

    RSI = 0b0110,
    RDI = 0b0111,
    R8  = 0b1000,
    R9  = 0b1001,
    R10 = 0b1010,
    R11 = 0b1011,

    // We also_lvalue don't allow R12 and R13 to be used as general purpose registers
    // Instead, reserve it for ourselves
    R12 = 0b1100, TMP_1_REG = 0b1100,
    R13 = 0b1101, TMP_2_REG = 0b1101,

    R14 = 0b1110,
    R15 = 0b1111,
};

typedef struct {
    enum Registers reg;
    uint8_t offset;
    bool is_reg;
} VLoc;

#define WRAP_NONE() ((VLoc) {.reg = NO_REG, .is_reg = true})
#define WRAP_REG(reg_) ((VLoc) {.reg = (reg_), .is_reg = true})
#define WRAP_VAR(offset_) ((VLoc) {.reg = SPILLED_REG, .offset = (offset_), .is_reg = false})
#define IS_ASSIGNED(loc_) ((loc_).reg != NO_REG)

bool loc_equal(VLoc loc_1, VLoc loc_2);
// endregion

typedef enum ValueType {
    TYPE_UNKNOWN,
    TYPE_CONFLICTING,
    TYPE_INT,
    TYPE_OBJECT,
} ValueType;

struct GetFunctionCallback {
    void* compiled_callback;
    void* ir_callback;
    void* jit_ptr;
};

// region Instruction
typedef union u_InstructionIR Instruction;
typedef Instruction* IRValue;

enum InstructionID {
    ID_INSTR_NONE = 0,
    ID_BLOCK_PARAMETER_IR,
    ID_INT_IR,
    ID_ADD_IR,
    ID_SUB_IR,
    ID_CMP_IR,
    ID_CALL_IR,
    ID_GLOBAL_IR,
    ID_GET_ATTR_IR,
    ID_GET_LOC_IR,
    ID_SET_LOC_IR,
    ID_NEW_OBJECT_IR,
};

struct InstructionBase  {
    VLoc loc;
    enum InstructionID id;
    uint16_t refs;
    uint16_t index;
    enum ValueType type;
};

struct ParameterIR {
    struct InstructionBase base;
    VLoc entry_loc;
    String var_name;
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

enum Comparison {
    IF_EQUAL = 0x84, IF_ZERO = 0x84,
    IF_NOT_EQUAL = 0x85, IF_NOT_ZERO = 0x85,
    IF_LESS = 0x8C,
    IF_LESS_EQUAL = 0x8E,
    IF_GREATER = 0x8F,
    IF_GREATER_EQUAL = 0x8D,
};

extern enum Comparison inverted_cmp[16];

#define INV_CMP(cmp) (inverted_cmp[(cmp)-0x80])

struct CompareIR {
    struct InstructionBase base;
    enum Comparison cmp;
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

struct GetAttrIR {
    struct InstructionBase base;
    Instruction* obj;
    String attr;
};

struct GetLocIR {
    struct InstructionBase base;
    Instruction* loc;
};

struct SetLocIR {
    struct InstructionBase base;
    Instruction* loc;
    Instruction* value;
};

struct NewObjectIR {
    struct InstructionBase base;
};

union u_InstructionIR {
    struct InstructionBase base;
    struct ParameterIR ir_parameter;
    struct IntIR ir_int;
    struct AddIR ir_add;
    struct SubIR ir_sub;
    struct CompareIR ir_cmp;
    struct CallIR ir_call;
    struct GlobalIR ir_global;
    struct GetAttrIR ir_get_attr;
    struct GetLocIR ir_get_loc;
    struct SetLocIR ir_set_loc;
    struct NewObjectIR ir_new_object;
};
// endregion

// region Terminator Base
enum TerminatorID {
    ID_TERM_NONE = 0,
    ID_RETURN_IR,
    ID_BRANCH_IR,
    ID_CBRANCH_IR,
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
};

struct CBranchIR {
    struct TerminatorBase base;
    IRValue cond;
    struct BlockIR* true_target;
    struct BlockIR* false_target;
};

union TerminatorIR {
    struct TerminatorBase ir_base;
    struct ReturnIR ir_return;
    struct BranchIR ir_branch;
    struct CBranchIR ir_cbranch;
};
// endregion

// region Block
struct BlockIR {
    LAList* first_instrs;
    LAList* last_instrs;
    uint16_t num_instrs;
    uint16_t num_params;
    uint32_t block_index;

    union TerminatorIR terminator;

    bool has_vars;
    struct HashTable variables;

    void* data;

    struct BlockIR* prev_block;
    struct BlockIR* next_block;
};
// endregion

// region Function
struct FunctionIR {
    String name;
    LAList* last_blocks;

    struct BlockIR* first_block;
    struct BlockIR* last_block;
    uint32_t num_blocks;

    void* compiled;
};
// endregion Function


#endif //OJIT_ASM_IR_H
