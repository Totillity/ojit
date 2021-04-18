#ifndef OJIT_OBJ_H
#define OJIT_OBJ_H

#include <stdint.h>

// ============ NAN Boxing ============
// Here are 64 bits:
//  0000000000000 000 000000000000000000000000000000000000000000000000
// |------A------|-B-|-----------------------C------------------------|
//   A: Check this. If its != 0, then its a double, otherwise its a boxed object.
//       Note: if it is a double, then you must invert the whole thing to restore it
//   B: Tag of the object (if it is one, otherwise its just double data)
//       Tags:
//           000: Pointer                           (uses 48 bits)
//           001: Unsigned Integer                  (uses 32 bits)
//           010-011: Unused
//           1xx: An Exception which was raised     (uses 48 bits)
//               100: Type error
//   C: Payload. Size and usage vary based on type.

typedef uint64_t OJITValue;

#define VAL_IS_DOUBLE(obj)       (((obj) >> 51) != 0)
#define VAL_IS_POINTER(obj)      (((obj) >> 48) == 0b000)
#define VAL_IS_ERROR(obj)        (((obj) >> 50) == 0b1)
#define VAL_IS_TYPE_ERROR(obj)   (((obj) >> 48) == 0b100)

#define VAL_IS_INT(obj) (((obj) >> 48) == 0b001)
#define VAL_AS_INT(val) ((uint32_t) ((val) & UINT32_MAX))
#define INT_AS_VAL(num) ((0b001ull << 48) | ((num) & UINT32_MAX))

#define VAL_AS_TYPE_ERROR(val) ((String) ((val) & ((2ull << 48) - 1)))

#endif //OJIT_OBJ_H
