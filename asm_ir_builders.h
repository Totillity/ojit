#ifndef OJIT_ASM_IR_BUILDERS_H
#define OJIT_ASM_IR_BUILDERS_H

#include "asm_ir.h"

// region Block Builders
IRValue block_add_parameter(struct BlockIR* block);


IRValue block_build_Int(struct BlockIR* block, int32_t constant);

IRValue block_build_Add(struct BlockIR* block, IRValue a, IRValue b);


void block_terminate_Return(struct BlockIR* block, IRValue value);

void block_terminate_Branch(struct BlockIR* block, struct BlockIR* target, size_t arg_count, IRValue* arguments);
// endregion

#endif //OJIT_ASM_IR_BUILDERS_H
