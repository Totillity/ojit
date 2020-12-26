#include <stdlib.h>
#include <string.h>

#include "asm_ir.h"

void init_instruction_list(struct InstructionList* list, size_t capacity) {
    list->array = malloc(sizeof(union InstructionIR) * capacity);
    if (list->array == NULL) {
        exit(-1);
    }
    list->cap = capacity;
    list->len = 0;
}


union InstructionIR* instruction_list_add_instruction(struct InstructionList* list) {
    if (list->len >= list->cap) {
        list->array = realloc(list->array, sizeof(union InstructionIR) * list->len * 2);
    }
    union InstructionIR* instr = &list->array[list->len++];

    return instr;
}


#define VALUE_LIST_INIT_SIZE (4)

void init_value_list(struct IRValueList* list, size_t item_count, IRValue* items) {
    if (item_count) {
        list->array = malloc(sizeof(union InstructionIR*) * item_count);
        if (list->array == NULL) {
            exit(-1);
        }
        memcpy(list->array, items, sizeof(union InstructionIR*) * item_count);
        list->cap = item_count;
        list->len = item_count;
    } else {
        list->array = malloc(sizeof(union InstructionIR*) * VALUE_LIST_INIT_SIZE);
        if (list->array == NULL) {
            exit(-1);
        }
        memcpy(list->array, items, sizeof(union InstructionIR*) * VALUE_LIST_INIT_SIZE);
        list->cap = VALUE_LIST_INIT_SIZE;
        list->len = 0;
    }

}

void value_list_add_value(struct IRValueList* list, IRValue value) {
    if (list->len >= list->cap) {
        list->array = realloc(list->array, sizeof(union InstructionIR) * list->len * 2);
    }
    list->array[list->len++] = value;
}

void init_block(struct BlockIR* block) {
    init_instruction_list(&block->instrs, 8);
    block->terminator.ir_base.id = ID_TERM_NONE;

    block->code_ptr = NULL;
    block->next_listener = NULL;
}

void init_block_list(struct BlockList* list, size_t capacity) {
    list->array = malloc(sizeof(struct BlockIR) * capacity);
    list->cap = capacity;
    list->len = 0;
}

struct BlockIR* block_list_add_block(struct BlockList* list) {
    if (list->len >= list->cap) {
        list->cap = list->len*2;
        list->array = realloc(list->array, sizeof(struct BlockIR) * list->cap);
    }
    struct BlockIR* new_block = &list->array[list->len++];
    init_block(new_block);
    return new_block;
}

struct FunctionIR* create_function(char* name) {
    struct FunctionIR* function = malloc(sizeof(struct FunctionIR));
    init_block_list(&function->blocks, 8);

    block_list_add_block(&function->blocks);
    return function;
}

struct BlockIR* function_add_block(struct FunctionIR* func) {
    return block_list_add_block(&func->blocks);
}
