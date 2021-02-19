#include <stdlib.h>
#include "ojit_state.h"

JState* create_jstate() {
    JState* jstate = malloc(sizeof(JState));
    jstate->ir_mem = create_mem_ctx();
    jstate->string_mem = create_mem_ctx();
    return jstate;
}

void destroy_jstate(JState* jstate) {
    destroy_mem_ctx(jstate->ir_mem);
    destroy_mem_ctx(jstate->string_mem);
    free(jstate);
}

FState* create_fstate(JState* jstate) {
    FState* fstate = malloc(sizeof(FState));
    fstate->parser_mem = create_mem_ctx();
    fstate->state = jstate;
    return fstate;
}

void destroy_fstate(FState* fstate) {
    destroy_mem_ctx(fstate->parser_mem);
    free(fstate);
}

CState* create_cstate(JState* jstate) {
    CState* bstate = malloc(sizeof(CState));
    bstate->state = jstate;
    bstate->compiler_mem = create_mem_ctx();
    return bstate;
}

void destroy_cstate(CState* cstate) {
    destroy_mem_ctx(cstate->compiler_mem);
    free(cstate);
}