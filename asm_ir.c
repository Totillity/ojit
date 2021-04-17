#include "asm_ir.h"

bool loc_equal(VLoc loc_1, VLoc loc_2) {
    if (loc_1.is_reg != loc_2.is_reg)
        return false;
    if (loc_1.is_reg == true)
        return loc_1.reg == loc_2.reg;
    return loc_1.offset == loc_2.offset;
}