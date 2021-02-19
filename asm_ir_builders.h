#ifndef OJIT_ASM_IR_BUILDERS_H
#define OJIT_ASM_IR_BUILDERS_H

#include "asm_ir.h"
#include "ojit_mem.h"

// region IRBuilder
struct IRBuilder {
    struct BlockIR* current_block;
    struct FunctionIR* function;
    MemCtx* ir_mem;
};

struct IRBuilder* create_builder(struct FunctionIR* function_ir, MemCtx* ir_mem);
struct BlockIR* builder_add_block(struct IRBuilder* builder);

void builder_goto_block(struct IRBuilder* builder, struct BlockIR* block_ir);
// endregion

// region Block Builders
IRValue builder_add_parameter(struct IRBuilder* builder);

IRValue builder_add_variable(struct IRBuilder* builder, String var_name, IRValue init_value);
IRValue builder_set_variable(struct IRBuilder* builder, String var_name, IRValue value);
IRValue builder_get_variable(struct IRBuilder* builder, String var_name);

IRValue builder_Int(struct IRBuilder* builder, int32_t constant);
IRValue builder_Add(struct IRBuilder* builder, IRValue a, IRValue b);
IRValue builder_Sub(struct IRBuilder* builder, IRValue a, IRValue b);

void builder_Return(struct IRBuilder* builder, IRValue value);
void builder_Branch(struct IRBuilder* builder, struct BlockIR* target, size_t arg_count, IRValue* arguments);
// endregion

#endif //OJIT_ASM_IR_BUILDERS_H
