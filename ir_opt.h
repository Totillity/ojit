#ifndef OJIT_IR_OPT_H
#define OJIT_IR_OPT_H

#include "asm_ir.h"

void ojit_optimize_func(struct FunctionIR* func, struct GetFunctionCallback callbacks);

#endif //OJIT_IR_OPT_H
