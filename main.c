#include <stdio.h>
#include <sys/time.h>

#include "jit_interpreter.h"
#include "obj.h"
#include "ojit_def.h"

typedef OJITValue (*FuncType)(OJITValue);

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
    bool success = jit_add_file(jit, "test.txt");
    if (success) {
        JITFunc main_func = jit_get_function(jit, "main", 4);
        jit_dump_function(jit, main_func, stdout);
        jit_dump_function(jit, jit_get_function(jit, "fibo", 4), stdout);

        OJITValue arg = INT_AS_VAL(20);
        OJITValue res = jit_call_function(jit, main_func, FuncType, arg);
        if (VAL_IS_TYPE_ERROR(res)) {
            ojit_new_error();
            ojit_build_error_chars("Function returned Error: ");
            ojit_build_error_String(VAL_AS_TYPE_ERROR(res));
            ojit_error();
        } else {
            printf("Function returned value: %i\n", VAL_AS_INT(res));
        }
    }
    return 0;
}
