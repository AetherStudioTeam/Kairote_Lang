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

 