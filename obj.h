#ifndef OJIT_OBJ_H
#define OJIT_OBJ_H

#include <stdint.h>

typedef uint64_t OJITObject;

#define OBJ_IS_DOUBLE(obj) (((obj) >> 51) != 0)
#define OBJ_IS_POINTER(obj) (((obj) >> 48) == 0)
#define OBJ_IS_INT(obj) (((obj) >> 48) == 0b001)

#define INT_AS_OBJ(val) ((0b001ull << 48) | ((val) & ((2ull << 32) - 1)))

#define OBJ_AS_INT(obj) ((uint32_t) ((obj) & ((2ull << 32) - 1)))

#endif //OJIT_OBJ_H
