#include "IrAssert.h"
#include <stdio.h>

bool KrtIrBuilderIsValid(KrtIRBuilder* builder) {
    if (!builder) return false;
    if (!builder->arena) return false;
    if (!builder->module) return false;
    return true;
}

bool KrtIrFunctionIsValid(KrtIRFunction* func) {
    if (!func) return false;
    if (!func->name) return false;
    if (func->param_count < 0) return false;
    return true;
}

bool KrtIrBlockIsValid(KrtIRBasicBlock* block) {
    if (!block) return false;
    if (!block->label) return false;
    return true;
}

bool KrtIrInstIsValid(KrtIRInst* inst) {
    if (!inst) return false;
    if (inst->opcode < KRT_IR_LOAD || inst->opcode > KRT_IR_NOP) return false;
    if (inst->operand_count < 0) return false;
    if (inst->operand_count > inst->operand_capacity) return false;
    return true;
}

bool KrtIrValueIsValid(KrtIRValue* value) {
    if (!value) return false;
    if (value->type < KRT_IR_VALUE_VOID || value->type > KRT_IR_VALUE_FUNCTION) return false;
    return true;
}

bool KrtIrArenaIsValid(KrtIRMemoryArena* arena) {
    if (!arena) return false;
    if (!arena->current_pool) return false;
    if (arena->pool_size == 0) return false;
    return true;
}

bool KrtIrOperandIndexIsValid(KrtIRInst* inst, int index) {
    if (!inst) return false;
    if (index < 0 || index >= inst->operand_count) return false;
    return true;
}

bool KrtIrParamIndexIsValid(KrtIRFunction* func, int index) {
    if (!func) return false;
    if (index < 0 || index >= func->param_count) return false;
    return true;
}

#if KRT_IR_ASSERT_ENABLED

static const char* KrtIrOpcodeToString(KrtIROpcode opcode) {
    switch (opcode) {
        case KRT_IR_LOAD: return "LOAD";
        case KRT_IR_STORE: return "STORE";
        case KRT_IR_ALLOC: return "ALLOC";
        case KRT_IR_IMM: return "IMM";
        case KRT_IR_ADD: return "ADD";
        case KRT_IR_SUB: return "SUB";
        case KRT_IR_MUL: return "MUL";
        case KRT_IR_DIV: return "DIV";
        case KRT_IR_MOD: return "MOD";
        case KRT_IR_AND: return "AND";
        case KRT_IR_OR: return "OR";
        case KRT_IR_XOR: return "XOR";
        case KRT_IR_LSHIFT: return "LSHIFT";
        case KRT_IR_RSHIFT: return "RSHIFT";
        case KRT_IR_POW: return "POW";
        case KRT_IR_LT: return "LT";
        case KRT_IR_GT: return "GT";
        case KRT_IR_EQ: return "EQ";
        case KRT_IR_LE: return "LE";
        case KRT_IR_GE: return "GE";
        case KRT_IR_NE: return "NE";
        case KRT_IR_JUMP: return "JUMP";
        case KRT_IR_BRANCH: return "BRANCH";
        case KRT_IR_CALL: return "CALL";
        case KRT_IR_RETURN: return "RETURN";
        case KRT_IR_LABEL: return "LABEL";
        case KRT_IR_STRCAT: return "STRCAT";
        case KRT_IR_CAST: return "CAST";
        case KRT_IR_LOADPTR: return "LOADPTR";
        case KRT_IR_STOREPTR: return "STOREPTR";
        case KRT_IR_ARRAY_STORE: return "ARRAY_STORE";
        case KRT_IR_INT_TO_STRING: return "INT_TO_STRING";
        case KRT_IR_DOUBLE_TO_STRING: return "DOUBLE_TO_STRING";
        case KRT_IR_COPY: return "COPY";
        case KRT_IR_NOP: return "NOP";
        default: return "UNKNOWN";
    }
}

static const char* KrtIrValueTypeToString(KrtIRValueType type) {
    switch (type) {
        case KRT_IR_VALUE_VOID: return "VOID";
        case KRT_IR_VALUE_IMM: return "IMM";
        case KRT_IR_VALUE_VAR: return "VAR";
        case KRT_IR_VALUE_TEMP: return "TEMP";
        case KRT_IR_VALUE_ARG: return "ARG";
        case KRT_IR_VALUE_STRING_CONST: return "STRING_CONST";
        case KRT_IR_VALUE_FUNCTION: return "FUNCTION";
        default: return "UNKNOWN";
    }
}

#endif 