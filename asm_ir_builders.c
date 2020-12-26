#include "asm_ir_builders.h"

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
