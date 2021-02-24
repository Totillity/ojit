#ifndef OJIT_ASM_IR_BUILDERS_H
#define OJIT_ASM_IR_BUILDERS_H

#include "asm_ir.h"
#include "ojit_mem.h"

// region IRBuilder
typedef struct {
    struct BlockIR* current_block;
    struct FunctionIR* function;
    MemCtx* ir_mem;
} IRBuilder;

IRBuilder* create_builder(struct FunctionIR* function_ir, MemCtx* ir_mem);
struct BlockIR* builder_add_block(IRBuilder* builder);

void builder_goto_block(IRBuilder* builder, struct BlockIR* block_ir);
// endregion

// region Block Builders
IRValue builder_add_parameter(IRBuilder* builder);

IRValue builder_add_variable(IRBuilder* builder, String var_name, IRValue init_value);
IRValue builder_set_variable(IRBuilder* builder, String var_name, IRValue value);
IRValue builder_get_variable(IRBuilder* builder, String var_name);

IRValue builder_Int(IRBuilder* builder, int32_t constant);
IRValue builder_Add(IRBuilder* builder, IRValue a, IRValue b);
IRValue builder_Sub(IRBuilder* builder, IRValue a, IRValue b);
IRValue builder_Global(IRBuilder* builder, String name);
IRValue builder_Call(IRBuilder* builder, IRValue callee);
void builder_Call_argument(IRValue call_instr, IRValue argument);

void builder_Return(IRBuilder* builder, IRValue value);
void builder_Branch(IRBuilder* builder, struct BlockIR* target, size_t arg_count, IRValue* arguments);
// endregion

void init_block(struct BlockIR* block, size_t block_num, MemCtx* ctx);

struct FunctionIR* create_function(String name, MemCtx* ctx);
struct BlockIR* function_add_block(struct FunctionIR* func, MemCtx* ctx);

#endif //OJIT_ASM_IR_BUILDERS_H
