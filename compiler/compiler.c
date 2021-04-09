#include <assert.h>
#include <stdio.h>

#include "../ojit_def.h"
#include "compiler.h"

#include "compiler_records.h"
#include "emit_instr.h"
#include "emit_terminator.h"

#ifdef OJIT_OPTIMIZATIONS
#include "../ir_opt.h"
#endif

// region Debug
int get_var_num(IRValue var, struct HashTable* table) {
    uint64_t ret;
    if (hash_table_get(table, HASH_KEY(var), &ret)) {
        return ret;
    } else {
        ret = table->len;
        hash_table_insert(table, HASH_KEY(var), ret);
        return ret;
    }
}


void dump_function(struct FunctionIR* func) {
    printf("FUNCTION ");
    int c_i = 0;
    while (c_i < func->name->length) {
        putchar(func->name->start_ptr[c_i]);
        c_i += 1;
    }
    printf("\n");

    struct HashTable var_names;
    MemCtx* tmp_mem = create_mem_ctx();
    init_hash_table(&var_names, tmp_mem);

    struct BlockIR* block = func->first_block;
    while (block) {
        printf("    BLOCK @%p\n", block);

        FOREACH_INSTR(instr, block->first_instrs) {
            int i = get_var_num(instr, &var_names);
#ifdef OJIT_READABLE_IR
            if (instr->base.refs == 0) {
                printf("        (LIKELY DISABLED) ");
            } else {
                printf("        ");
            }
#else
            printf("        ");
#endif
            switch (instr->base.id) {
                case ID_INT_IR: {
                    printf("$%i = INT32 %d\n", i, instr->ir_int.constant);
                    break;
                }
                case ID_BLOCK_PARAMETER_IR: {
                    if (instr->ir_parameter.var_name) {
                        printf("$%i = PARAMETER \"", i);

                        int c_ = 0;
                        while (c_ < instr->ir_parameter.var_name->length) {
                            putchar(instr->ir_parameter.var_name->start_ptr[c_]);
                            c_ += 1;
                        }
                        printf("\"\n");
                    } else {
                        printf("$%i = PARAMETER (DISABLED)\n", i);
                    }
                    break;
                }
                case ID_ADD_IR: {
                    printf("$%i = ADD $%i, $%i\n", i, get_var_num(instr->ir_add.a, &var_names), get_var_num(instr->ir_add.b, &var_names));
                    break;
                }
                case ID_SUB_IR: {
                    printf("$%i = SUB $%i, $%i\n", i, get_var_num(instr->ir_sub.a, &var_names), get_var_num(instr->ir_sub.b, &var_names));
                    break;
                }
                case ID_CMP_IR: {
                    printf("$%i = CMP (%i) $%i, $%i\n", i, instr->ir_cmp.cmp, get_var_num(instr->ir_cmp.a, &var_names), get_var_num(instr->ir_cmp.b, &var_names));
                    break;
                }
                case ID_CALL_IR: {
                    printf("$%i = CALL\n", i);
                    break;
                }
                case ID_GLOBAL_IR: {
                    printf("$%i = GLOBAL\n", i);
                    break;
                }
                case ID_NEW_OBJECT_IR: {
                    printf("$%i = NEW_OBJECT\n", i);
                    break;
                }
                case ID_GET_ATTR_IR: {
                    printf("$%i = GETATTR $%i\n", i, get_var_num(instr->ir_get_attr.obj, &var_names));
                    break;
                }
                case ID_GET_LOC_IR: {
                    printf("$%i = GETLOC $%i\n", i, get_var_num(instr->ir_get_loc.loc, &var_names));
                    break;
                }
                case ID_SET_LOC_IR: {
                    printf("$%i = SETLOC $%i, $%i\n", i, get_var_num(instr->ir_set_loc.loc, &var_names), get_var_num(instr->ir_set_loc.value, &var_names));
                    break;
                }
                case ID_INSTR_NONE: {
                    printf("$%i = UNKNOWN\n", i);
                    break;
                }
            }
        }
        switch (block->terminator.ir_base.id) {
            case ID_BRANCH_IR: {
                printf("        BRANCH @%p\n", block->terminator.ir_branch.target);
                break;
            }
            case ID_CBRANCH_IR: {
                printf("        CBRANCH $%i (true: @%p, false: @%p)\n",
                       get_var_num(block->terminator.ir_cbranch.cond, &var_names),
                       block->terminator.ir_cbranch.true_target,
                       block->terminator.ir_cbranch.false_target);
                break;
            }
            case ID_RETURN_IR: {
                printf("        RETURN $%i\n", get_var_num(block->terminator.ir_return.value, &var_names));
                break;
            }
            default: {
                printf("        UNKNOWN\n");
            }
        }

        block = block->next_block;
    }
    destroy_mem_ctx(tmp_mem);
}
// endregion

// region Compile
void assign_function_parameters(struct FunctionIR* func) {
    struct BlockIR* first_block = func->first_block;
    int param_num = 0;
    FOREACH_INSTR(instr, first_block->first_instrs) {
        if (instr->base.id == ID_BLOCK_PARAMETER_IR) {
            enum Register64 reg;
            switch (param_num) {
                case 0: reg = RCX; break;
                case 1: reg = RDX; break;
                case 2: reg = R8; break;
                case 3: reg = R9; break;
                default: exit(-1);  // TODO
            }
            instr->ir_parameter.entry_reg = reg;
            param_num += 1;
        }
    }
}


struct CompiledFunction stitch_segments(Segment* first_segment, MemCtx* ctx) {
    Segment* segment = first_segment;
    uint32_t offset = 0;
    while (segment) {
        segment->base.offset_from_start = offset;
        offset += segment->base.max_size;
        segment = segment->base.next_segment;
    }

    segment = first_segment;
    uint32_t saved_space = 0;
    while (segment) {
        segment->base.offset_from_start -= saved_space;
        switch (segment->base.type) {
            case SEGMENT_JUMP: {
                uint32_t jump_to = segment->jump.jump_to->base.offset_from_start;
                if (jump_to > segment->base.offset_from_start + segment->base.max_size) {
                    jump_to -= saved_space;
                }
                int32_t jump_dist = jump_to - (segment->base.offset_from_start + segment->base.max_size);
                if (jump_dist == 0) {
                    segment->base.final_size = 0;
                    saved_space += segment->base.max_size;
                } else if (jump_dist > -256 && jump_dist < 255) {
                    segment->base.final_size = 2;
                    saved_space += segment->base.max_size - 2;
                } else {
                    segment->base.final_size = segment->base.max_size;
                }
                break;
            }
            default: break;
        }
        segment = segment->base.next_segment;
    }

    uint8_t* mem = ojit_alloc(ctx, offset);
    segment = first_segment;
    while (segment) {
        uint8_t* write_ptr = mem + segment->base.offset_from_start;
        switch (segment->base.type) {
            case SEGMENT_LABEL: {
                break;
            }
            case SEGMENT_CODE: {
                ojit_memcpy(write_ptr, segment->code.code + (512 - segment->base.max_size), segment->base.max_size);
                break;
            }
            case SEGMENT_JUMP: {
                struct SegmentJump* jump = &segment->jump;
                int32_t jump_dist = jump->jump_to->base.offset_from_start - (jump->base.offset_from_start + jump->base.final_size);
                if (jump->base.final_size == 0) {

                } else if (jump->base.final_size == 2) {
                    write_ptr[0] = jump->short_form[0];
                    write_ptr[1] = (uint8_t) jump_dist;
                } else {
                    if (jump->base.max_size == 5) {
                        write_ptr[0] = jump->long_form[0];
                        write_ptr += 1;
                    } else {
                        write_ptr[0] = jump->long_form[0];
                        write_ptr[1] = jump->long_form[1];
                        write_ptr += 2;
                    }
                    write_ptr[0] = (uint8_t) (jump_dist >>  0) & 0xFF;
                    write_ptr[1] = (uint8_t) (jump_dist >>  8) & 0xFF;
                    write_ptr[2] = (uint8_t) (jump_dist >> 16) & 0xFF;
                    write_ptr[3] = (uint8_t) (jump_dist >> 24) & 0xFF;
                }
                break;
            }
        }
        segment = segment->base.next_segment;
    }
    return (struct CompiledFunction) {.mem = mem, .size = offset - saved_space};
}


void ojit_jit_error(uint32_t val) {
    printf("Error: %i\n", val);
}


struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback) {
#ifdef OJIT_OPTIMIZATIONS
    ojit_optimize_func(func, callback);
#endif
    dump_function(func);

    assign_function_parameters(func);

    struct BlockIR* block = func->first_block;
    Segment* first_label;
    Segment* prev_label;
    first_label = prev_label = create_segment_label(NULL, NULL, compiler_mem);
    while (block) {
        prev_label = create_segment_label(prev_label, NULL, compiler_mem);
        block->data = prev_label;
        block = block->next_block;
    }
    Segment* errs_label = create_segment_label(prev_label, NULL, compiler_mem);
    Segment* err_return_label = create_segment_label(errs_label, NULL, compiler_mem);

    struct AssemblerState state;
    state.writer.write_mem = compiler_mem;
    state.jit_mem = create_mem_ctx(); // TODO bring this out
    state.callback = callback;
    state.errs_label = errs_label;
    state.err_return_label = err_return_label;

    state.writer.curr = create_segment_code(err_return_label, NULL, compiler_mem);
    state.writer.label = err_return_label;
    asm_emit_call_r64(RAX, &state.writer);
    asm_emit_sub_r64_i32(RSP, 32, &state.writer);
    asm_emit_mov_r64_i64(RAX, (uint64_t) ojit_jit_error, &state.writer);

    block = func->first_block;
    Segment* segment = NULL;
    while (block) {
        struct BlockIR* next_block = block->next_block;
        segment = create_segment_code(block->data, next_block ? next_block->data : errs_label, compiler_mem);
        init_asm_state(&state, block, block->data, segment);

        emit_terminator(&block->terminator, &state);

        LAListIter instr_iter;
        lalist_init_iter(&instr_iter, block->last_instrs, block->last_instrs->len, sizeof(Instruction));
        Instruction* instr = lalist_iter_prev(&instr_iter);
        int k = 0;
        while (instr) {
            emit_instruction(instr, &state);
            instr = lalist_iter_prev(&instr_iter);
            k += 1;
        }

        block = block->next_block;
    }

    return stitch_segments(first_label, compiler_mem);
}

#ifdef WIN32
#include <windows.h>
void* copy_to_executable(void* from, size_t len) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    void* mem = VirtualAlloc(NULL, info.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    if (mem == NULL) {
        ojit_new_error();
        ojit_build_error_chars("Failed to move generated code to executable memory.\n");
        ojit_error();
        exit(0);
    }
    memcpy(mem, from, len);

    DWORD dummy;
    bool success = VirtualProtect(mem, len, PAGE_EXECUTE_READ, &dummy);
    if (!success) {
        ojit_new_error();
        ojit_build_error_chars("Failed to move generated code to executable memory.\n");
        ojit_error();
        exit(0);
    }

    return mem;
}
#else
#warning Executing jited code is currently unsupported on platforms other than Windows.
void* copy_to_executable(CState* bstate, void* from, size_t len) {
    ojit_fatal_error(bstate->error, "Executing jit'ed code is currently unsupported on platforms other than Windows.\n");
}
#endif
// endregion
