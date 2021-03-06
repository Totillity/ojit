#ifndef OJIT_COMPILER_H
#define OJIT_COMPILER_H

#include <stdint.h>
#include "../asm_ir.h"

struct CompiledFunction {
    uint8_t* mem;
    size_t size;
};

struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback);
void* copy_to_executable(void* from, size_t len);
#endif //OJIT_COMPILER_H
