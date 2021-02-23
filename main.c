#include <stdio.h>
#include <sys/time.h>

#include "jit_interpreter.h"

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
    JIT* jit = ojit_create_jit();
    jit_add_file(jit, "test.txt");
    JITFunc main_func = jit_get_function(jit, "main", 4);
    jit_dump_function(jit, main_func, stdout);
    int res = jit_call_function(jit, main_func, FuncType, 3);
    printf("Value: %i\n", res);
    return 0;
}
