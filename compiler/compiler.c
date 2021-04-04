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


#define GET_RECORD(block) ((struct BlockRecord*) (block)->data)

struct CompiledFunction ojit_compile_function(struct FunctionIR* func, MemCtx* compiler_mem, struct GetFunctionCallback callback) {
#ifdef OJIT_OPTIMIZATIONS
    ojit_optimize_func(func, callback);
#endif
    dump_function(func);

    struct AssemblerState state;
    state.ctx = compiler_mem;
    state.jit_mem = create_mem_ctx(); // TODO bring this out

    size_t generated_size = 0;

    assign_function_parameters(func);

    struct BlockIR* block = func->first_block;
    while (block) {
        union MemBlock* end_mem = create_mem_block_code(NULL, NULL, compiler_mem);

        init_asm_state(&state, block, end_mem, callback);

        emit_terminator(&block->terminator, &state);

        LAListIter instr_iter;
        lalist_init_iter(&instr_iter, block->last_instrs, sizeof(Instruction));
        lalist_iter_position(&instr_iter, block->last_instrs->len);
        Instruction* instr = lalist_iter_prev(&instr_iter);
        int k = 0;
        while (instr) {
            emit_instruction(instr, &state);
            instr = lalist_iter_prev(&instr_iter);
            k += 1;
        }

        generated_size += state.block_size;

        struct BlockRecord* record = ojit_alloc(compiler_mem, sizeof(struct BlockRecord));
        record->end_mem = end_mem;
        record->max_offset_from_end = generated_size;
        record->actual_offset_from_end = 0;
        block->data = record;

        block = block->next_block;
    }

    uint8_t* func_mem = ojit_alloc(compiler_mem, generated_size);
    uint8_t* end_ptr = func_mem + generated_size;
    uint8_t* write_ptr = end_ptr;

    struct MemBlockJump* last_visited_jump = NULL;
    block = func->last_block;
    while (block) {
        struct BlockRecord* record = GET_RECORD(block);
        union MemBlock* curr_mem_block = record->end_mem;
        while (curr_mem_block) {
            if (curr_mem_block->base.is_code) {
                write_ptr -= curr_mem_block->base.len;
                ojit_memcpy(write_ptr, curr_mem_block->code.code + (512 - curr_mem_block->base.len), curr_mem_block->base.len);
            } else {
                struct BlockRecord* target_record = GET_RECORD(curr_mem_block->jump.target);
                uint32_t curr_offset = end_ptr - write_ptr;
                int32_t jump_dist;
                if (target_record->actual_offset_from_end != 0) { // TODO account for the extra bytes
                    jump_dist = target_record->actual_offset_from_end - curr_offset;
                } else {
                    jump_dist = target_record->max_offset_from_end - curr_offset;
                }
#ifdef OJIT_OPTIMIZATIONS
                if (jump_dist <= 256 && jump_dist >= -256) {
                    curr_mem_block->jump.is_short = true;
                    write_ptr -= 2;
                    ojit_memcpy(write_ptr, curr_mem_block->jump.short_form, 2);
                } else {
                    curr_mem_block->jump.is_short = false;
                    write_ptr -= curr_mem_block->base.len;
                    ojit_memcpy(write_ptr, curr_mem_block->jump.long_form + (6 - curr_mem_block->jump.base.len), curr_mem_block->base.len);
                }
#else
                curr_mem_block->jump.is_short = false;
                write_ptr -= curr_mem_block->base.len;
                ojit_memcpy(write_ptr, curr_mem_block->jump.long_form + (6 - curr_mem_block->jump.base.len), curr_mem_block->base.len);
#endif
                curr_mem_block->jump.offset_from_end = end_ptr - write_ptr;
                curr_mem_block->jump.next_jump = last_visited_jump;
                if (last_visited_jump) last_visited_jump->prev_jump = (struct MemBlockJump*) curr_mem_block;
                last_visited_jump = (struct MemBlockJump*) curr_mem_block;
            }
            curr_mem_block = curr_mem_block->base.prev_block;
        }
        record->actual_offset_from_end = end_ptr - write_ptr;
        block = block->prev_block;
    }

    struct MemBlockJump* curr_jump = last_visited_jump;
    while (curr_jump) {
        if (curr_jump->is_short) {
            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + 2;
            uint32_t jump_dist = (curr_jump->offset_from_end - 2) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
            *(ptr - 1) = jump_dist & 0xFF;
        } else {
            uint8_t* ptr = end_ptr - curr_jump->offset_from_end + curr_jump->base.len;
            uint32_t jump_dist = (curr_jump->offset_from_end - curr_jump->base.len) - GET_RECORD(curr_jump->target)->actual_offset_from_end;
            *(ptr - 1) = (jump_dist >> 24) & 0xFF;
            *(ptr - 2) = (jump_dist >> 16) & 0xFF;
            *(ptr - 3) = (jump_dist >> 8) & 0xFF;
            *(ptr - 4) = (jump_dist >> 0) & 0xFF;
        }
        curr_jump = curr_jump->next_jump;
    }

    uint32_t final_size = end_ptr - write_ptr;

    return (struct CompiledFunction) {write_ptr, final_size};
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
