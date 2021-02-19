#include "ojit_err.h"
#include "asm_ir_builders.h"


struct IRBuilder* create_builder(struct FunctionIR* function_ir, MemCtx* ctx) {
    struct IRBuilder* builder = ojit_alloc(ctx, sizeof(struct IRBuilder));
    builder->function = function_ir;
    builder->ir_mem = ctx;
    builder->current_block = lalist_get(function_ir->first_blocks, sizeof(union InstructionIR), 0);
    return builder;
}

union InstructionIR* builder_add_instr(struct IRBuilder* builder) {
    if (builder->current_block->last_instrs->len + sizeof(union InstructionIR) > LALIST_BLOCK_SIZE) {
        builder->current_block->last_instrs = lalist_grow(builder->ir_mem, builder->current_block->last_instrs, NULL);
    }
    return lalist_add(builder->current_block->last_instrs, sizeof(union InstructionIR));
}


struct BlockIR* builder_add_block(struct IRBuilder* builder) {
    return function_add_block(builder->function, builder->ir_mem);
}


void builder_goto_block(struct IRBuilder* builder, struct BlockIR* block_ir) {
    builder->current_block = block_ir;
}


IRValue builder_add_parameter(struct IRBuilder* builder) {
    struct ParameterIR* instr = &builder_add_instr(builder)->ir_parameter;
    instr->base.id = ID_PARAMETER_IR;
    instr->base.reg = NO_REG;
    return (IRValue) instr;
}


IRValue builder_add_variable(struct IRBuilder* builder, String var_name, IRValue init_value) {
    bool was_set = hash_table_insert(&builder->current_block->variables, var_name, (uint64_t) init_value);
    OJIT_ASSERT(was_set, "");
    return init_value;
}


IRValue builder_set_variable(struct IRBuilder* builder, String var_name, IRValue value) {
    bool was_set = hash_table_set(&builder->current_block->variables, var_name, (uint64_t) value);
    if (!was_set) {
        ojit_error("Variable already exists");
    }
    return value;
}


IRValue builder_get_variable(struct IRBuilder* builder, String var_name) {
    IRValue value = NULL;
    bool was_get = hash_table_get(&builder->current_block->variables, var_name, (uint64_t*) &value);
    if (!was_get) {
        ojit_error("Variable does not exist");
    }
    // TODO
    return value;
}


IRValue builder_Int(struct IRBuilder* builder, int32_t constant) {
    struct IntIR* instr = &builder_add_instr(builder)->ir_int;
    instr->constant = constant;
    instr->base.id = ID_INT_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

IRValue builder_Add(struct IRBuilder* builder, IRValue a, IRValue b) {
    struct AddIR* instr = &builder_add_instr(builder)->ir_add;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_ADD_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

IRValue builder_Sub(struct IRBuilder* builder, IRValue a, IRValue b) {
    struct SubIR* instr = &builder_add_instr(builder)->ir_sub;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_SUB_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

void builder_Return(struct IRBuilder* builder, IRValue value) {
    struct ReturnIR* term = &builder->current_block->terminator.ir_return;
    term->value = value;
    term->base.id = ID_RETURN_IR;
}

void builder_Branch(struct IRBuilder* builder, struct BlockIR* target, size_t arg_count, IRValue* arguments) {
    struct BranchIR* term = &builder->current_block->terminator.ir_branch;
    term->target = target;
    term->arguments = ojit_alloc(builder->ir_mem, sizeof(IRValue) * arg_count);
    ojit_memcpy(term->arguments, arguments, sizeof(IRValue) * arg_count);
    term->base.id = ID_BRANCH_IR;
}
