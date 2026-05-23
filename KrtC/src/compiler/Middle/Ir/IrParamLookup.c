#include "Ir.h"
#include "IrParamTable.h"

KrtIRParamNode* KrtIrFunctionFindParam(KrtIRFunction* func, const char* name) {
    if (!func || !name) return NULL;
    
    if (func->param_table) {
        return KrtIrParamTableFind(func->param_table, name);
    }
    
    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].name && strcmp(func->params[i].name, name) == 0) {
            
            static KrtIRParamNode temp_node;
            temp_node.name = func->params[i].name;
            temp_node.type = func->params[i].type;
            temp_node.index = i;
            temp_node.next = NULL;
            return &temp_node;
        }
    }
    
    return NULL;
}

int KrtIrFunctionGetParamIndex(KrtIRFunction* func, const char* name) {
    KrtIRParamNode* param = KrtIrFunctionFindParam(func, name);
    return param ? param->index : -1;
}