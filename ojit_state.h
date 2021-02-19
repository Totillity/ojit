#ifndef OJIT_OJIT_STATE_H
#define OJIT_OJIT_STATE_H

#include "ojit_mem.h"
#include "ojit_err.h"

typedef struct s_JState {
    MemCtx* ir_mem;
    MemCtx* string_mem;
} JState;

JState* create_jstate();
void destroy_jstate(JState* jstate);

typedef struct s_FState {
    JState* state;
    MemCtx* parser_mem;
} FState;

FState* create_fstate(JState* jstate);
void destroy_fstate(FState* fstate);

typedef struct s_CState {
    JState* state;
    MemCtx* compiler_mem;
} CState;

CState* create_cstate(JState* jstate);
void destroy_cstate(CState* cstate);

#endif //OJIT_OJIT_STATE_H
