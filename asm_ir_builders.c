#include "ojit_err.h"
#include "asm_ir_builders.h"


IRBuilder* create_builder(struct FunctionIR* function_ir, MemCtx* ctx) {
IRBuilder* builder = ojit_alloc(ctx, sizeof(IRBuilder));
    builder->function = function_ir;
    builder->ir_mem = ctx;
    builder->current_block = lalist_get(function_ir->first_blocks, sizeof(Instruction), 0);
    return builder;
}

Instruction* builder_add_instr(IRBuilder* builder) {
    if (builder->current_block->last_instrs->len + sizeof(Instruction) > LALIST_BLOCK_SIZE) {
        builder->current_block->last_instrs = lalist_grow(builder->ir_mem, builder->current_block->last_instrs, NULL);
    }
    builder->current_block->num_instrs++;
    return lalist_add(builder->current_block->last_instrs, sizeof(Instruction));
}


struct BlockIR* builder_add_block(IRBuilder* builder) {
    return function_add_block(builder->function, builder->ir_mem);
}


void builder_goto_block(IRBuilder* builder, struct BlockIR* block_ir) {
    builder->current_block = block_ir;
}


IRValue builder_add_parameter(IRBuilder* builder) {
    struct ParameterIR* instr = &builder_add_instr(builder)->ir_parameter;
    instr->base.id = ID_PARAMETER_IR;
    instr->base.reg = NO_REG;
    return (IRValue) instr;
}


IRValue builder_add_variable(IRBuilder* builder, String var_name, IRValue init_value) {
    bool was_set = hash_table_insert(&builder->current_block->variables, var_name, (uint64_t) init_value);
    OJIT_ASSERT(was_set, "e");
    return init_value;
}


IRValue builder_set_variable(IRBuilder* builder, String var_name, IRValue value) {
    bool was_set = hash_table_set(&builder->current_block->variables, var_name, (uint64_t) value);
    OJIT_ASSERT(was_set, "Variable already exists");
    return value;
}


IRValue builder_get_variable(IRBuilder* builder, String var_name) {
    IRValue value = NULL;
    bool was_get = hash_table_get(&builder->current_block->variables, var_name, (uint64_t*) &value);
    OJIT_ASSERT(was_get, "Variable does not exist");
    return value;
}


IRValue builder_Int(IRBuilder* builder, int32_t constant) {
    struct IntIR* instr = &builder_add_instr(builder)->ir_int;
    instr->constant = constant;
    instr->base.id = ID_INT_IR;
    instr->base.reg = NO_REG;
    return (Instruction*) instr;
}

IRValue builder_Add(IRBuilder* builder, IRValue a, IRValue b) {
    struct AddIR* instr = &builder_add_instr(builder)->ir_add;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_ADD_IR;
    instr->base.reg = NO_REG;
    return (Instruction*) instr;
}

IRValue builder_Sub(IRBuilder* builder, IRValue a, IRValue b) {
    struct SubIR* instr = &builder_add_instr(builder)->ir_sub;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_SUB_IR;
    instr->base.reg = NO_REG;
    return (Instruction*) instr;
}

IRValue builder_Call(IRBuilder* builder, IRValue callee) {
    struct CallIR* instr = &builder_add_instr(builder)->ir_call;
    instr->callee = callee;
    instr->arguments = lalist_grow(builder->ir_mem, NULL, NULL);
    instr->base.id = ID_CALL_IR;
    instr->base.reg = NO_REG;
    return (Instruction*) instr;
}

void builder_Call_argument(IRValue call_instr, IRValue argument) {
    struct CallIR* instr = &call_instr->ir_call;
    Instruction** ptr = lalist_add(instr->arguments, sizeof(IRValue));
    *ptr = argument;
}

IRValue builder_Global(IRBuilder* builder, String name) {
    struct GlobalIR* instr = &builder_add_instr(builder)->ir_global;
    instr->name = name;
    instr->base.id = ID_GLOBAL_IR;
    instr->base.reg = NO_REG;
    return (Instruction*) instr;
}

void builder_Return(IRBuilder* builder, IRValue value) {
    struct ReturnIR* term = &builder->current_block->terminator.ir_return;
    term->value = value;
    term->base.id = ID_RETURN_IR;
}

void builder_Branch(IRBuilder* builder, struct BlockIR* target, size_t arg_count, IRValue* arguments) {
    struct BranchIR* term = &builder->current_block->terminator.ir_branch;
    term->target = target;
    term->arguments = ojit_alloc(builder->ir_mem, sizeof(IRValue) * arg_count);
    ojit_memcpy(term->arguments, arguments, sizeof(IRValue) * arg_count);
    term->base.id = ID_BRANCH_IR;
}
