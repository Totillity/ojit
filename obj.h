#ifndef OJIT_OBJ_H
#define OJIT_OBJ_H

#include <stdint.h>

typedef uint64_t OJITValue;

#define VAL_IS_DOUBLE(obj) (((obj) >> 51) != 0)
#define VAL_IS_POINTER(obj) (((obj) >> 48) == 0)
#define VAL_IS_INT(obj) (((obj) >> 48) == 0b001)
#define VAL_IS_ERROR(obj) (((obj) >> 50) == 0b1)

#define INT_AS_VAL(num) ((0b001ull << 48) | ((num) & UINT32_MAX))

#define VAL_AS_INT(val) ((uint32_t) ((val) & UINT32_MAX))

#endif //OJIT_OBJ_H
