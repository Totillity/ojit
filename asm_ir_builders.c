#include "ojit_def.h"
#include "asm_ir_builders.h"

enum Comparison inverted_cmp[16] = {
        [IF_EQUAL-0x80] = 0x85,
        [IF_NOT_EQUAL-0x80] = 0x84,
        [IF_LESS-0x80] = 0x8D,
        [IF_LESS_EQUAL-0x80] = 0x8F,
        [IF_GREATER-0x80] = 0x8E,
        [IF_GREATER_EQUAL-0x80] = 0x8C,
};

void init_block(struct BlockIR* block, uint32_t index, MemCtx* ctx) {
    block->first_instrs = block->last_instrs = lalist_grow(ctx, NULL, NULL);
    block->num_instrs = 0;

    block->terminator.ir_base.id = ID_TERM_NONE;

    block->data = NULL;

    block->has_vars = false;
    init_hash_table(&block->variables, ctx);
}

struct BlockIR* function_add_block(struct FunctionIR* func, MemCtx* ctx) {
    struct BlockIR* block = lalist_grow_add(&func->last_blocks, sizeof(struct BlockIR));
    init_block(block, func->num_blocks++, ctx);
    return block;
}

IRBuilder* create_builder(struct FunctionIR* function_ir, MemCtx* ctx) {
    IRBuilder* builder = ojit_alloc(ctx, sizeof(IRBuilder));
    builder->function = function_ir;
    builder->ir_mem = ctx;
    builder->current_block = function_ir->first_block;
    return builder;
}

Instruction* builder_add_instr(IRBuilder* builder) {
    Instruction* instr = lalist_grow_add(&builder->current_block->last_instrs, sizeof(Instruction));
    instr->base.reg = NO_REG;
    instr->base.refs = 0;
    instr->base.index = builder->current_block->num_instrs++;
    return instr;
}


struct BlockIR* builder_add_block(IRBuilder* builder, struct BlockIR* after_block) {
    struct BlockIR* block = function_add_block(builder->function, builder->ir_mem);
    if (builder->function->last_block == after_block) {
        builder->function->last_block = block;
    }
    block->prev_block = after_block;
    if (after_block) {
        block->next_block = after_block->next_block;
        if (after_block->next_block) {
            after_block->next_block->prev_block = block;
        }
        after_block->next_block = block;
    }
    return block;
}


void builder_temp_swap_block(IRBuilder* builder, struct BlockIR* block_ir) {
    builder->current_block = block_ir;
}

void builder_enter_block(IRBuilder* builder, struct BlockIR* block_ir) {
    builder->current_block = block_ir;
    FOREACH_INSTR(curr_instr, block_ir->first_instrs) {
        if (INSTR_TYPE(curr_instr) == ID_BLOCK_PARAMETER_IR) {
            struct ParameterIR* param = &curr_instr->ir_parameter;
            builder_add_variable(builder, param->var_name, (IRValue) param);
        } else {
            break;
        }
    }
}

IRValue builder_add_parameter(IRBuilder* builder, String var_name) {
    struct ParameterIR* instr = &builder_add_instr(builder)->ir_parameter;
    instr->var_name = var_name;
    instr->entry_reg = NO_REG;
    INSTR_TYPE(instr) = ID_BLOCK_PARAMETER_IR;
    return (IRValue) instr;
}


IRValue builder_add_variable(IRBuilder* builder, String var_name, IRValue init_value) {
    bool was_set = hash_table_insert(&builder->current_block->variables, STRING_KEY(var_name), (uint64_t) init_value);
    OJIT_ASSERT(was_set, "e");
    return init_value;
}


IRValue builder_set_variable(IRBuilder* builder, String var_name, IRValue value) {
    bool was_set = hash_table_set(&builder->current_block->variables, STRING_KEY(var_name), (uint64_t) value);
    OJIT_ASSERT(was_set, "Variable already exists");
    return value;
}


IRValue builder_get_variable(IRBuilder* builder, String var_name) {
    IRValue value = NULL;
    bool was_get = hash_table_get(&builder->current_block->variables, STRING_KEY(var_name), (uint64_t*) &value);
    OJIT_ASSERT(was_get, "Variable does not exist");
    return value;
}


// region Build Instruction
IRValue builder_Int(IRBuilder* builder, int32_t constant) {
    struct IntIR* instr = &builder_add_instr(builder)->ir_int;
    instr->constant = constant;
    INSTR_TYPE(instr) = ID_INT_IR;
    return (Instruction*) instr;
}

IRValue builder_Add(IRBuilder* builder, IRValue a, IRValue b) {
    struct AddIR* instr = &builder_add_instr(builder)->ir_add;
    instr->a = a;
    instr->b = b;
    INC_INSTR(a);
    INC_INSTR(b);
    INSTR_TYPE(instr) = ID_ADD_IR;
    return (Instruction*) instr;
}

IRValue builder_Sub(IRBuilder* builder, IRValue a, IRValue b) {
    struct SubIR* instr = &builder_add_instr(builder)->ir_sub;
    instr->a = a;
    instr->b = b;
    INC_INSTR(a);
    INC_INSTR(b);
    INSTR_TYPE(instr) = ID_SUB_IR;
    return (Instruction*) instr;
}

IRValue builder_Cmp(IRBuilder* builder, enum Comparison cmp, IRValue a, IRValue b) {
    struct CompareIR* instr = &builder_add_instr(builder)->ir_cmp;
    instr->cmp = cmp;
    instr->a = a;
    instr->b = b;
    INC_INSTR(a);
    INC_INSTR(b);
    INSTR_TYPE(instr) = ID_CMP_IR;
    return (Instruction*) instr;
}

IRValue builder_Call(IRBuilder* builder, IRValue callee) {
    struct CallIR* instr = &builder_add_instr(builder)->ir_call;
    instr->callee = callee;
    instr->arguments = lalist_grow(builder->ir_mem, NULL, NULL);
    INSTR_TYPE(instr) = ID_CALL_IR;
    return (Instruction*) instr;
}

void builder_Call_argument(IRValue call_instr, IRValue argument) {
    struct CallIR* instr = &call_instr->ir_call;
    Instruction** ptr = lalist_grow_add(&instr->arguments, sizeof(IRValue));
    *ptr = argument;
    INC_INSTR(argument);
}

IRValue builder_Global(IRBuilder* builder, String name) {
    struct GlobalIR* instr = &builder_add_instr(builder)->ir_global;
    instr->name = name;
    INSTR_TYPE(instr) = ID_GLOBAL_IR;
    return (Instruction*) instr;
}

IRValue builder_GetAttrIR(IRBuilder* builder, Instruction* obj, String attr) {
    struct GetAttrIR* instr = &builder_add_instr(builder)->ir_get_attr;
    instr->obj = obj;
    instr->attr = attr;
    INSTR_TYPE(instr) = ID_GET_ATTR_IR;
    INC_INSTR(obj);
    return (Instruction*) instr;
}

IRValue builder_GetLocIR(IRBuilder* builder, Instruction* loc) {
    struct GetLocIR* instr = &builder_add_instr(builder)->ir_get_loc;
    instr->loc = loc;
    INSTR_TYPE(instr) = ID_GET_LOC_IR;
    INC_INSTR(loc);
    return (Instruction*) instr;
}

IRValue builder_SetLocIR(IRBuilder* builder, Instruction* loc, Instruction* value) {
    struct SetLocIR* instr = &builder_add_instr(builder)->ir_set_loc;
    instr->loc = loc;
    instr->value = value;
    INSTR_TYPE(instr) = ID_SET_LOC_IR;
    instr->base.refs = 1;
    INC_INSTR(loc);
    INC_INSTR(value);
    return (Instruction*) instr;
}

IRValue builder_NewObjectIR(IRBuilder* builder) {
    struct NewObjectIR* instr = &builder_add_instr(builder)->ir_new_object;
    INSTR_TYPE(instr) = ID_NEW_OBJECT_IR;
    return (IRValue) instr;
}
// endregion

void builder_Return(IRBuilder* builder, IRValue value) {
    struct ReturnIR* term = &builder->current_block->terminator.ir_return;
    term->value = value;
    term->base.id = ID_RETURN_IR;
    INC_INSTR(value);
}

void merge_blocks(IRBuilder* builder, struct BlockIR* to, struct BlockIR* from) {
    if (to->has_vars) {
        FOREACH_INSTR(curr_instr, to->first_instrs) {
            if (INSTR_TYPE(curr_instr) == ID_BLOCK_PARAMETER_IR) {
                struct ParameterIR* param = &curr_instr->ir_parameter;
                if (param->var_name == NULL) continue;
                IRValue arg;
                if (!hash_table_get(&from->variables, STRING_KEY(param->var_name), (uint64_t*) &arg)){
                    param->var_name = NULL;
                } else {
                    INC_INSTR(arg);
                }
            } else {
                break;
            }
        }
    } else {
        struct BlockIR* original_block = builder->current_block;
        builder_temp_swap_block(builder, to);
        TableEntry* curr_entry = from->variables.last_entry;
        while (curr_entry) {
            builder_add_parameter(builder, curr_entry->key.cmp_obj);
            IRValue arg = (void*) curr_entry->value;
            INC_INSTR(arg);
            curr_entry = curr_entry->prev;
        }
        builder_temp_swap_block(builder, original_block);
        to->has_vars = true;
    }
}

void builder_Branch(IRBuilder* builder, struct BlockIR* target) {
    struct BranchIR* term = &builder->current_block->terminator.ir_branch;
    term->target = target;

    term->base.id = ID_BRANCH_IR;

    merge_blocks(builder, target, builder->current_block);
}

void builder_CBranch(IRBuilder* builder, IRValue cond, struct BlockIR* true_target, struct BlockIR* false_target) {
    struct CBranchIR* term = &builder->current_block->terminator.ir_cbranch;
    term->cond = cond;
    INC_INSTR(cond);
    term->true_target = true_target;
    term->false_target = false_target;
    term->base.id = ID_CBRANCH_IR;

    merge_blocks(builder, true_target, builder->current_block);
    merge_blocks(builder, false_target, builder->current_block);
}

struct FunctionIR* create_function(String name, MemCtx* ctx) {
    struct FunctionIR* function = ojit_alloc(ctx, sizeof(struct FunctionIR));
    function->name = name;
    function->compiled = NULL;
    function->last_blocks = lalist_grow(ctx, NULL, NULL);
    function->first_block = function->last_block = function_add_block(function, ctx);
    function->first_block->prev_block = NULL;
    function->last_block->next_block = NULL;
    return function;
}