#include <stdlib.h>
#include <stdio.h>
#include "asm_ir_builders.h"


struct IRBuilder* create_builder(struct FunctionIR* function_ir) {
    struct IRBuilder* builder = malloc(sizeof(struct IRBuilder));
    builder->function = function_ir;
    builder->current_block = GET_BLOCK(function_ir, 0);
    return builder;
}


struct BlockIR* builder_add_block(struct IRBuilder* builder) {
    return function_add_block(builder->function);
}


void builder_goto_block(struct IRBuilder* builder, struct BlockIR* block_ir) {
    builder->current_block = block_ir;
}


IRValue builder_add_parameter(struct IRBuilder* builder) {
    struct ParameterIR* instr = &instruction_list_add_instruction(&builder->current_block->instrs)->ir_parameter;
    instr->base.id = ID_PARAMETER_IR;
    instr->base.reg = NO_REG;
    return (IRValue) instr;
}


IRValue builder_add_variable(struct IRBuilder* builder, String var_name, IRValue init_value) {
    bool was_set = hash_table_insert(&builder->current_block->variables, var_name, (uint64_t) init_value);
    if (!was_set) {
        printf("Cannot add variable");
        exit(-1); // TODO exception handling
    }
    return init_value;
}


IRValue builder_set_variable(struct IRBuilder* builder, String var_name, IRValue value) {
    bool was_set = hash_table_set(&builder->current_block->variables, var_name, (uint64_t) value);
    if (!was_set) {
        printf("Cannot set variable");
        exit(-1); // TODO exception handling
    }
    return value;
}


IRValue builder_get_variable(struct IRBuilder* builder, String var_name) {
    IRValue value = NULL;
    bool was_get = hash_table_get(&builder->current_block->variables, var_name, (uint64_t*) &value);
    if (!was_get) {
        printf("Cannot get variable");
        exit(-1); // TODO exception handling
    }
    return value;
}


IRValue builder_Int(struct IRBuilder* builder, int32_t constant) {
    struct IntIR* instr = &instruction_list_add_instruction(&builder->current_block->instrs)->ir_int;
    instr->constant = constant;
    instr->base.id = ID_INT_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

IRValue builder_Add(struct IRBuilder* builder, IRValue a, IRValue b) {
    struct AddIR* instr = &instruction_list_add_instruction(&builder->current_block->instrs)->ir_add;
    instr->a = a;
    instr->b = b;
    instr->base.id = ID_ADD_IR;
    instr->base.reg = NO_REG;
    return (union InstructionIR*) instr;
}

IRValue builder_Sub(struct IRBuilder* builder, IRValue a, IRValue b) {
    struct SubIR* instr = &instruction_list_add_instruction(&builder->current_block->instrs)->ir_sub;
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
    init_value_list(&term->arguments, arg_count, arguments);
    term->offset_from_end = 0;
    term->next_listener = NULL;
    term->in_block = builder->current_block;
    term->base.id = ID_BRANCH_IR;
}
