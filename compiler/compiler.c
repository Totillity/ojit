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
                int32_t jump_dist = segment->jump.jump_to->base.offset_from_start - segment->base.offset_from_start;
                if (jump_dist > -256 && jump_dist < 255) {
                    segment->jump.is_short = true;
                    saved_space += segment->base.max_size - 2;
                } else {
                    segment->jump.is_short = false;
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
                int32_t jump_dist = segment->jump.jump_to->base.offset_from_start - segment->base.offset_from_start;
                if (jump->is_short) {
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


struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback) {
#ifdef OJIT_OPTIMIZATIONS
    ojit_optimize_func(func, callback);
#endif
    dump_function(func);

    assign_function_parameters(func);

    struct AssemblerState state;
    state.ctx = compiler_mem;
    state.jit_mem = create_mem_ctx(); // TODO bring this out
    state.callback = callback;
    state.segment_count = 0;

    size_t generated_size = 0;

    struct BlockIR* block = func->first_block;
    Segment* first_label;
    Segment* prev_label;
    first_label = prev_label = create_segment_label(NULL, NULL, compiler_mem);
    while (block) {
        prev_label = create_segment_label(prev_label, NULL, compiler_mem);
        block->data = prev_label;
        block = block->next_block;
    }

    block = func->first_block;
    Segment* segment = NULL;
    while (block) {
        segment = create_mem_block_code(block->data, segment, compiler_mem);
        init_asm_state(&state, block, segment, block->data);

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

        generated_size += state.block_size;
        block = block->next_block;
    }

    uint8_t* func_mem = ojit_alloc(compiler_mem, generated_size);
    uint8_t* end_ptr = func_mem + generated_size;
    uint8_t* write_ptr = end_ptr;

    return stitch_segments(first_label, compiler_mem);
//
//    struct SegmentJump* curr_jump = last_visited_jump;
//    while (curr_jump) {
//        if (curr_jump->is_short) {
//            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + 2;
//            uint32_t jump_dist = (curr_jump->offset_from_end - 2) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
//            *(ptr - 1) = jump_dist & 0xFF;
//        } else {
//            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + curr_jump->base.len;
//            uint32_t jump_dist = (curr_jump->offset_from_end - curr_jump->base.len) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
//            *(ptr - 1) = (jump_dist >> 24) & 0xFF;
//            *(ptr - 2) = (jump_dist >> 16) & 0xFF;
//            *(ptr - 3) = (jump_dist >> 8) & 0xFF;
//            *(ptr - 4) = (jump_dist >> 0) & 0xFF;
//        }
//        curr_jump = curr_jump->next_jump;
//    }
//
//    uint32_t final_size = end_ptr - write_ptr;

//    return (struct CompiledFunction) {write_ptr, 2000}; // TODO
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
