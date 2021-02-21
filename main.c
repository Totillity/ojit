#include <stdio.h>
#include <sys/time.h>

//#include "parser/parser.h"
#include "program_ir.h"

typedef int (*FuncType)(int);


double time_function(FuncType func, int arg) {
    int iterations = 100000;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        func(arg);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_in_nsec = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    return time_in_nsec / iterations;
}



//    for (int i = 0; i < main->blocks.len; i++) {
//        struct BlockIR* item = main->blocks.array[i];
//    }

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

int main() {
    CompilerManager* program_ir = create_CompilerManager();

    String source = CompilerManager_read_file(program_ir, "test.txt");
    CompilerManager_parse_source(program_ir, source);

    struct CompiledFunction compiled = CompilerManager_compile_function(program_ir, "main", 4);
    for (int i = 0; i < compiled.size; i++ ) {
        printf("%02x", compiled.mem[i]);
    }
    printf("\n");

    FuncType func = (FuncType) copy_to_executable(compiled.mem, compiled.size);

    printf("Single time in nsec: %f\n", time_function(func, 3));

    printf("Value: %i\n", func(3));

    return 0;
}
