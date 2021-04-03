#include <stdio.h>
#include <sys/time.h>

#include "jit_interpreter.h"
#include "obj.h"

typedef OJITObject (*FuncType)(OJITObject);

double time_function(JIT* jit, JITFunc func, int arg) {
    int iterations = 1000000000;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        jit_call_function(jit, func, FuncType, arg);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_in_nsec = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    return time_in_nsec / iterations;
}

int main() {
    JIT* jit = ojit_create_jit();
    jit_add_file(jit, "test.txt");
    JITFunc main_func = jit_get_function(jit, "main", 4);
    jit_dump_function(jit, main_func, stdout);

    OJITObject res = jit_call_function(jit, main_func, FuncType, 4);

//    double t = time_function(jit, main_func, 3);
//    printf("Value: %i, Time: %f ns\n", res, time_in_nsec);
    printf("Value: %llu\n", res);
    return 0;
}
