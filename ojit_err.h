#ifndef OJIT_OJIT_ERR_H
#define OJIT_OJIT_ERR_H

#include "ojit_def.h"
#include "string_tools/ojit_string.h"

#ifdef OJIT_SKIP_CHECKS
#define OJIT_ASSERT(cond, err_msg) do {} while (0)
#else
#define OJIT_ASSERT(cond, err_msg) do { if (!(cond)) {ojit_new_error(); ojit_build_error_chars(err_msg); ojit_error(); exit(0); }; } while (0)
#endif


void ojit_new_error();
void ojit_build_error_String(String str);
void ojit_build_error_chars(char* str);
void ojit_build_error_char(char c);
void ojit_build_error_int(int i);

void ojit_error();

_Noreturn void exit(int code);

#endif //OJIT_OJIT_ERR_H