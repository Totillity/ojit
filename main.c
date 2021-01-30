#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

//#include "parser/parser.h"
#include "program_ir.h"
#include "compiler.h"


typedef int (*FuncType)();


int main() {
    ProgramIR program_ir;
    create_program_ir(&program_ir);

    String source = program_ir_read_file(&program_ir, "test.txt");
    program_ir_parse_source(&program_ir, source);
    struct FunctionIR* main;
    program_ir_get_function_r(&program_ir, "main", 4, &main);


//    // region Construct IR
//    struct FunctionIR* function = create_function(STRING("main"));
//
//    struct BlockIR* entry = GET_BLOCK(function, 0);
//
//    struct BlockIR* end   = function_add_block(function);
//    IRValue param_1 = block_add_parameter(end);
//    IRValue param_2 = block_add_parameter(end);
//
//    IRValue const_1 = block_build_Int(entry, 1);
//    IRValue const_2 = block_build_Int(entry, 2);
//    IRValue arguments[2] = {const_1, const_2};
//    block_terminate_Branch(entry, end, 2, arguments);
//
//    IRValue added   = block_build_Add(end, param_1, param_2);
//    block_terminate_Return(end, added);
//    // endregion

    struct CompiledFunction compiled = compile_function(main);
    for (int i = 0; i < compiled.size; i++ ) {
        printf("%02x", compiled.mem[i]);
    }
    printf("\n");

    FuncType func = (FuncType) copy_to_executable(compiled.mem, compiled.size);
    printf("Value: %i\n", func());

    return 0;
}
