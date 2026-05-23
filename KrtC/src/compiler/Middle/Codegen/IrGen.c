#define _POSIX_C_SOURCE 200809L
#include <stddef.h>

extern size_t strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);
extern char *strstr(const char *haystack, const char *needle);

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include "../Ir/Ir.h"
#include "../../compiler.h"
#include "../../../Core/Utils/OutputCache.h"
#include "../../../Core/Utils/KrtCommon.h"
#include "../../Frontend/FrontendTemp/FrontendTemp/semantic/TypeChecker.h"
#include "../../Frontend/FrontendTemp/FrontendTemp/semantic/NameMangling.h"

static KrtIRValue KrtIrGenerateExpression(KrtIRBuilder* builder, ASTNode* expr);
static void KrtIrGenerateStatement(KrtIRBuilder* builder, ASTNode* stmt);
static void KrtIrGenerateBlock(KrtIRBuilder* builder, ASTNode* block);
static int KrtIrCheckHasReturn(ASTNode* node);
static void KrtIrEnsureMainEntry(KrtIRBuilder* builder);
static void KrtIrPushClassContext(KrtIRBuilder* builder, const char* class_name);
static void KrtIrPopClassContext(KrtIRBuilder* builder);
static const char* KrtIrCurrentClassContext(KrtIRBuilder* builder);
static char* KrtIrMangleStaticMember(const char* class_name, const char* member_name);
static int KrtIrEvaluateNumericConstant(ASTNode* expr, double* out_value);
static void KrtIrPushNamespaceContext(KrtIRBuilder* builder, const char* namespace_name);
static void KrtIrPopNamespaceContext(KrtIRBuilder* builder);
static const char** KrtIrGetNamespacePath(KrtIRBuilder* builder, int* out_count);

static void KrtIrFillParamTypesForMangle(ASTNode** args, int count, KrtTokenType* out) {
    for (int i = 0; i < count; i++) {
        ASTNode* a = args ? args[i] : NULL;
        if (!a) {
            out[i] = TOKEN_INT32;
            continue;
        }
        switch (a->type) {
            case AST_STRING:
                out[i] = TOKEN_STRING;
                break;
            case AST_BOOLEAN:
                out[i] = TOKEN_BOOL;
                break;
            case AST_NUMBER:
                out[i] = TOKEN_INT32;
                break;
            default:
                out[i] = TOKEN_INT32;
                break;
        }
    }
}

static char* KrtIrMangleCallFunctionName(const char** ns_path, const char* base_name,
                                         ASTNode** call_args, int arg_count) {
    if (!base_name) {
        return NULL;
    }
    KrtTokenType buf[32];
    int n = arg_count;
    if (n > (int)(sizeof(buf) / sizeof(buf[0]))) {
        n = (int)(sizeof(buf) / sizeof(buf[0]));
    }
    if (n > 0 && call_args) {
        KrtIrFillParamTypesForMangle(call_args, n, buf);
        return name_mangle_function(ns_path, base_name, buf, n);
    }
    return name_mangle_function(ns_path, base_name, NULL, 0);
}

static char* KrtIrMangleStaticMember(const char* class_name, const char* member_name) {
    return name_mangle_simple(class_name, member_name);
}

static void KrtIrPushNamespaceContext(KrtIRBuilder* builder, const char* namespace_name) {
    if (!builder || !namespace_name) {
        return;
    }
    if (builder->namespace_stack_size >= builder->namespace_stack_capacity) {
        int new_capacity = builder->namespace_stack_capacity == 0 ? 8 : builder->namespace_stack_capacity * 2;
        char** new_stack = (char**)KRT_REALLOC(builder->namespace_stack, new_capacity * sizeof(char*));
        if (!new_stack) {
            return;
        }
        builder->namespace_stack = new_stack;
        builder->namespace_stack_capacity = new_capacity;
    }
    builder->namespace_stack[builder->namespace_stack_size] = KRT_STRDUP(namespace_name);
    builder->namespace_stack_size++;
}

static void KrtIrPopNamespaceContext(KrtIRBuilder* builder) {
    if (!builder || builder->namespace_stack_size <= 0) {
        return;
    }
    builder->namespace_stack_size--;
    if (builder->namespace_stack[builder->namespace_stack_size]) {
        KRT_FREE(builder->namespace_stack[builder->namespace_stack_size]);
        builder->namespace_stack[builder->namespace_stack_size] = NULL;
    }
}

static const char** KrtIrGetNamespacePath(KrtIRBuilder* builder, int* out_count) {
    if (!builder || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    if (builder->namespace_stack_size <= 0) {
        *out_count = 0;
        return NULL;
    }
    
    const char** path = (const char**)KRT_MALLOC(sizeof(const char*) * (size_t)(builder->namespace_stack_size + 1));
    if (!path) {
        *out_count = 0;
        return NULL;
    }
    
    for (int i = 0; i < builder->namespace_stack_size; i++) {
        path[i] = builder->namespace_stack[i];
    }
    path[builder->namespace_stack_size] = NULL;
    *out_count = builder->namespace_stack_size;
    
    return path;
}

static bool is_string_expression(KrtIRBuilder* builder, ASTNode* expr) {
    if (!expr) {
        return false;
    }
    if (expr->type == AST_STRING) {
        return true;
    }
    if (expr->type == AST_IDENTIFIER) {
        return false;
    }
    if (expr->type == AST_BINARY_OPERATION && expr->data.binary_op.operator == TOKEN_PLUS) {
        return is_string_expression(builder, expr->data.binary_op.left) ||
               is_string_expression(builder, expr->data.binary_op.right);
    }
    return false;
}

static void KrtIrEnsureMainEntry(KrtIRBuilder* builder) {
    if (!builder) {
        return;
    }
    if (!builder->module->main_function) {
        
        KrtIRFunction* main_func = KrtIrFunctionCreate(builder, "_ZN4mainEv", NULL, 0, TOKEN_VOID);
        builder->module->main_function = main_func;
        KrtIrFunctionSetEntry(builder, main_func);
    } else {
        KrtIrFunctionSetEntry(builder, builder->module->main_function);
    }
    KrtIRFunction* func = builder->module->main_function;
    if (!func->entry_block) {
        KrtIRBasicBlock* entry = KrtIrBlockCreate(builder, "entry");
        KrtIrBlockSetCurrent(builder, entry);
    } else {
        
        KrtIrBlockSetCurrent(builder, func->entry_block);
    }
}

static void KrtIrPushClassContext(KrtIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) {
        return;
    }
    if (builder->class_stack_size >= builder->class_stack_capacity) {
        int new_capacity = builder->class_stack_capacity == 0 ? 8 : builder->class_stack_capacity * 2;
        char** new_stack = (char**)KRT_REALLOC(builder->class_name_stack, new_capacity * sizeof(char*));
        if (!new_stack) {
            return;
        }
        builder->class_name_stack = new_stack;
        builder->class_stack_capacity = new_capacity;
    }
    builder->class_name_stack[builder->class_stack_size] = KRT_STRDUP(class_name);
    builder->class_stack_size++;
}

static void KrtIrPopClassContext(KrtIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) {
        return;
    }
    builder->class_stack_size--;
    if (builder->class_name_stack[builder->class_stack_size]) {
        KRT_FREE(builder->class_name_stack[builder->class_stack_size]);
        builder->class_name_stack[builder->class_stack_size] = NULL;
    }
}

static const char* KrtIrCurrentClassContext(KrtIRBuilder* builder) {
    if (!builder || builder->class_stack_size <= 0) {
        return NULL;
    }
    return builder->class_name_stack[builder->class_stack_size - 1];
}

static int KrtIrEvaluateNumericConstant(ASTNode* expr, double* out_value) {
    if (!expr || !out_value) {
        return 0;
    }
    switch (expr->type) {
        case AST_NUMBER:
            *out_value = expr->data.number_value;
            return 1;
        case AST_BOOLEAN:
            *out_value = expr->data.boolean_value ? 1 : 0;
            return 1;
        case AST_BINARY_OPERATION: {
            double left_val, right_val;
            if (KrtIrEvaluateNumericConstant(expr->data.binary_op.left, &left_val) &&
                KrtIrEvaluateNumericConstant(expr->data.binary_op.right, &right_val)) {
                switch (expr->data.binary_op.operator) {
                    case TOKEN_PLUS: *out_value = left_val + right_val; return 1;
                    case TOKEN_MINUS: *out_value = left_val - right_val; return 1;
                    case TOKEN_MULTIPLY: *out_value = left_val * right_val; return 1;
                    case TOKEN_DIVIDE: 
                        if (right_val != 0) {
                            *out_value = left_val / right_val;
                            return 1;
                        }
                        return 0;
                    case TOKEN_BITWISE_AND: *out_value = (double)((long long)left_val & (long long)right_val); return 1;
                    case TOKEN_BITWISE_OR: *out_value = (double)((long long)left_val | (long long)right_val); return 1;
                    case TOKEN_BITWISE_XOR: *out_value = (double)((long long)left_val ^ (long long)right_val); return 1;
                    case TOKEN_LSHIFT: *out_value = (double)((long long)left_val << (long long)right_val); return 1;
                    case TOKEN_RSHIFT: *out_value = (double)((long long)left_val >> (long long)right_val); return 1;
                    case TOKEN_POWER: *out_value = pow(left_val, right_val); return 1;
                    default: return 0;
                }
            }
            return 0;
        }
        case AST_UNARY_OPERATION: {
            double val;
            if (KrtIrEvaluateNumericConstant(expr->data.unary_op.operand, &val)) {
                switch (expr->data.unary_op.operator) {
                    case TOKEN_MINUS: *out_value = -val; return 1;
                    case TOKEN_NOT: *out_value = (val == 0) ? 1 : 0; return 1;
                    default: return 0;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

static bool try_extract_constant(ASTNode* node, double* value) {
    if (!node || !value) {
        return false;
    }
    if (node->type == AST_NUMBER) {
        *value = node->data.number_value;
        return true;
    }
    return false;
}

static bool is_float_expression(KrtIRBuilder* builder, ASTNode* expr) {
    if (!expr) return false;
    if (expr->type == AST_NUMBER) return true;
    if (expr->type == AST_IDENTIFIER) {
        
        if (builder->type_context && builder->type_context->current_scope) {
            TypeCheckSymbolTable* scope = builder->type_context->current_scope;
            while (scope) {
                TypeCheckSymbol* symbol = type_check_symbol_table_lookup(scope, expr->data.identifier_name);
                if (symbol && symbol->type) {
                    return symbol->type->kind == TYPE_FLOAT64 || symbol->type->kind == TYPE_FLOAT32;
                }
                scope = scope->parent;
            }
        }
        return false;
    }
    if (expr->type == AST_CALL) {
        if (expr->data.call.name && strcmp(expr->data.call.name, "timer_elapsed") == 0) {
            return true;
        }
    }
    return false;
}

static KrtIRValue convert_to_string_if_needed(KrtIRBuilder* builder, ASTNode* node, KrtIRValue value) {
    if (is_string_expression(builder, node)) {
        return value;
    }
    if (is_float_expression(builder, node)) {
        return KrtIrDoubleToString(builder, value);
    }
    return KrtIrIntToString(builder, value);
}

static KrtIRValue KrtIrGeneratePrint(KrtIRBuilder* builder, ASTNode** values, int value_count, bool has_newline) {
    KrtIRValue final_str;
    bool first = true;
    
    if (value_count == 0) {
        if (has_newline) {
            KrtIRValue empty_str;
            empty_str.type = KRT_IR_VALUE_STRING_CONST;
            empty_str.data.string_const_id = -1;
            
            KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
            if (args) {
                args[0] = empty_str;
                KrtIrCall(builder, "Console__WriteLine", args, 1);
                KRT_FREE(args);
            }
        }
        KrtIRValue void_val = {0};
        void_val.type = KRT_IR_VALUE_VOID;
        return void_val;
    }

    for (int i = 0; i < value_count; i++) {
        KrtIRValue val = KrtIrGenerateExpression(builder, values[i]);
        KrtIRValue str_val = convert_to_string_if_needed(builder, values[i], val);
        
        if (first) {
            final_str = str_val;
            first = false;
        } else {
            final_str = KrtIrStrcat(builder, final_str, str_val);
        }
    }
    
    KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
    if (args) {
        args[0] = final_str;
        const char* func_name = has_newline ? "Console__WriteLine" : "Console__Write";
        KrtIRValue result = KrtIrCall(builder, func_name, args, 1);
        KRT_FREE(args);
        return result;
    }
    
    KrtIRValue void_val = {0};
    void_val.type = KRT_IR_VALUE_VOID;
    return void_val;
}

static KrtIRValue KrtIrGenerateExpression(KrtIRBuilder* builder, ASTNode* expr) {
    if (!expr) {
        KrtIRValue void_val = {0};
        void_val.type = KRT_IR_VALUE_VOID;
        return void_val;
    }

    double const_val;
    if (KrtIrEvaluateNumericConstant(expr, &const_val)) {
        return KrtIrImm(builder, const_val);
    }

    switch (expr->type) {
        case AST_IDENTIFIER: {
            const char* var_name = expr->data.identifier_name;
            if (builder->current_function && builder->current_function->params) {
                for (int i = 0; i < builder->current_function->param_count; i++) {
                    if (strcmp(builder->current_function->params[i].name, var_name) == 0) {
                        return KrtIrArg(builder, i);
                    }
                }
            }
            const char* current_class = KrtIrCurrentClassContext(builder);
            if (current_class && var_name) {
                char* mangled = KrtIrMangleStaticMember(current_class, var_name);
                if (mangled) {
                    KrtIRGlobal* global = KrtIrModuleFindGlobal(builder->module, mangled);
                    if (global) {
                        KrtIRValue v = KrtIrLoad(builder, mangled);
                        KRT_FREE(mangled);
                        return v;
                    }
                    KRT_FREE(mangled);
                }
            }
            return KrtIrLoad(builder, var_name);
        }

        case AST_NUMBER: {
            return KrtIrImm(builder, expr->data.number_value);
        }

        case AST_BOOLEAN: {
            return KrtIrImm(builder, expr->data.boolean_value ? 1 : 0);
        }

        case AST_STRING: {
            return KrtIrStringConst(builder, expr->data.string_value);
        }

        case AST_NEW_EXPRESSION: {
            const char* class_name = expr->data.new_expr.class_name;
            int layout_size = KrtIrLayoutGetSize(builder, class_name);
            double alloc_size = (double)layout_size;
            KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
            if (!args) {
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }
            args[0] = KrtIrImm(builder, alloc_size);
            KrtIRValue obj = KrtIrCall(builder, "KrtMalloc", args, 1);
            KRT_FREE(args);
            char* ctor_name = KrtIrMangleStaticMember(class_name, "constructor");
            if (ctor_name) {
                bool exists = false;
                KrtIRFunction* f = builder->module->functions;
                while (f) {
                    if (strcmp(f->name, ctor_name) == 0) {
                        exists = true;
                        break;
                    }
                    f = f->next;
                }
                if (!exists) {
                    KrtIrFunctionCreate(builder, ctor_name, NULL, -1, TOKEN_VOID);
                }
                int ac = expr->data.new_expr.argument_count;
                KrtIRValue* cargs = (KrtIRValue*)KRT_MALLOC((ac + 1) * sizeof(KrtIRValue));
                if (cargs) {
                    cargs[0] = obj;
                    for (int i = 0; i < ac; i++) {
                        cargs[i + 1] = KrtIrGenerateExpression(builder, expr->data.new_expr.arguments[i]);
                    }
                    KrtIrCall(builder, ctor_name, cargs, ac + 1);
                    KRT_FREE(cargs);
                }
                KRT_FREE(ctor_name);
            }
            return obj;
        }
        case AST_BINARY_OPERATION: {
            KrtIRValue lhs = KrtIrGenerateExpression(builder, expr->data.binary_op.left);
            KrtIRValue rhs = KrtIrGenerateExpression(builder, expr->data.binary_op.right);
            switch (expr->data.binary_op.operator) {
                case TOKEN_ASSIGN: {
                    if (expr->data.binary_op.left->type == AST_IDENTIFIER) {
                        const char* name = expr->data.binary_op.left->data.identifier_name;
                        KrtIrStore(builder, name, rhs);
                        return rhs;
                    }
                    if (expr->data.binary_op.left->type == AST_MEMBER_ACCESS) {
                        ASTNode* objexpr = expr->data.binary_op.left->data.member_access.object;
                        const char* member_name = expr->data.binary_op.left->data.member_access.member_name;
                        if (objexpr->type == AST_THIS) {
                            const char* current_class = KrtIrCurrentClassContext(builder);
                            int offset = KrtIrLayoutGetOffset(builder, current_class, member_name);
                            KrtIRValue this_val = KrtIrArg(builder, 0);
                            KrtIrStorePtr(builder, this_val, offset >= 0 ? offset : 0, rhs);
                            return rhs;
                        }
                        if (objexpr->type == AST_IDENTIFIER) {
                            const char* class_name = objexpr->data.identifier_name;
                            char* mangled = KrtIrMangleStaticMember(class_name, member_name);
                            if (mangled) {
                                KrtIrStore(builder, mangled, rhs);
                                KRT_FREE(mangled);
                                return rhs;
                            }
                        }
                        if (expr->data.binary_op.left->data.member_access.resolved_class_name) {
                            int offset = KrtIrLayoutGetOffset(builder, expr->data.binary_op.left->data.member_access.resolved_class_name, member_name);
                            KrtIRValue base = KrtIrGenerateExpression(builder, objexpr);
                            KrtIrStorePtr(builder, base, offset >= 0 ? offset : 0, rhs);
                            return rhs;
                        }
                    }
                    return rhs;
                }
                case TOKEN_PLUS: {
                    bool left_is_string = is_string_expression(builder, expr->data.binary_op.left);
                    bool right_is_string = is_string_expression(builder, expr->data.binary_op.right);
                    if (left_is_string || right_is_string) {
                        KrtIRValue string_lhs = convert_to_string_if_needed(builder, expr->data.binary_op.left, lhs);
                        KrtIRValue string_rhs = convert_to_string_if_needed(builder, expr->data.binary_op.right, rhs);
                        return KrtIrStrcat(builder, string_lhs, string_rhs);
                    }
                    return KrtIrAdd(builder, lhs, rhs);
                }
                case TOKEN_MINUS:
                    return KrtIrSub(builder, lhs, rhs);
                case TOKEN_MULTIPLY:
                    return KrtIrMul(builder, lhs, rhs);
                case TOKEN_DIVIDE:
                    return KrtIrDiv(builder, lhs, rhs);
                case TOKEN_MODULO:
                    return KrtIrMod(builder, lhs, rhs);
                case TOKEN_BITWISE_AND:
                    return KrtIrAnd(builder, lhs, rhs);
                case TOKEN_BITWISE_OR:
                    return KrtIrOr(builder, lhs, rhs);
                case TOKEN_BITWISE_XOR:
                    return KrtIrXor(builder, lhs, rhs);
                case TOKEN_LSHIFT:
                    return KrtIrLshift(builder, lhs, rhs);
                case TOKEN_RSHIFT:
                    return KrtIrRshift(builder, lhs, rhs);
                case TOKEN_POWER:
                    return KrtIrPow(builder, lhs, rhs);
                case TOKEN_LESS:
                    return KrtIrCompare(builder, KRT_IR_LT, lhs, rhs);
                case TOKEN_GREATER:
                    return KrtIrCompare(builder, KRT_IR_GT, lhs, rhs);
                case TOKEN_EQUAL:
                    return KrtIrCompare(builder, KRT_IR_EQ, lhs, rhs);
                case TOKEN_LESS_EQUAL:
                    return KrtIrCompare(builder, KRT_IR_LE, lhs, rhs);
                case TOKEN_GREATER_EQUAL:
                    return KrtIrCompare(builder, KRT_IR_GE, lhs, rhs);
                case TOKEN_NOT_EQUAL:
                    return KrtIrCompare(builder, KRT_IR_NE, lhs, rhs);
                case TOKEN_AND: {
                    
                    static int and_label_id = 0;
                    char rhs_label[64], end_label[64], result_var[32];
                    snprintf(rhs_label, sizeof(rhs_label), "and_rhs_%d", and_label_id);
                    snprintf(end_label, sizeof(end_label), "and_end_%d", and_label_id);
                    snprintf(result_var, sizeof(result_var), "and_result_%d", and_label_id++);
                    
                    KrtIRBasicBlock* rhs_block = KrtIrBlockCreate(builder, rhs_label);
                    KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
                    KrtIRBasicBlock* current_block = builder->current_block;
                    
                    KrtIrStore(builder, result_var, KrtIrImm(builder, 0));
                    
                    KrtIrBranch(builder, lhs, rhs_block, end_block);
                    if (current_block) current_block->next = rhs_block;
                    rhs_block->next = end_block;
                    
                    KrtIrBlockSetCurrent(builder, rhs_block);
                    KrtIrStore(builder, result_var, rhs);
                    KrtIrJump(builder, end_block);
                    
                    KrtIrBlockSetCurrent(builder, end_block);
                    return KrtIrLoad(builder, result_var);
                }
                case TOKEN_OR: {
                    
                    static int or_label_id = 0;
                    char rhs_label[64], end_label[64], result_var[32];
                    snprintf(rhs_label, sizeof(rhs_label), "or_rhs_%d", or_label_id);
                    snprintf(end_label, sizeof(end_label), "or_end_%d", or_label_id);
                    snprintf(result_var, sizeof(result_var), "or_result_%d", or_label_id++);
                    
                    KrtIRBasicBlock* rhs_block = KrtIrBlockCreate(builder, rhs_label);
                    KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
                    KrtIRBasicBlock* current_block = builder->current_block;
                    
                    KrtIrStore(builder, result_var, KrtIrImm(builder, 1));
                    
                    KrtIrBranch(builder, lhs, end_block, rhs_block);
                    if (current_block) current_block->next = end_block;
                    rhs_block->next = end_block;
                    
                    KrtIrBlockSetCurrent(builder, rhs_block);
                    KrtIrStore(builder, result_var, rhs);
                    KrtIrJump(builder, end_block);
                    
                    KrtIrBlockSetCurrent(builder, end_block);
                    return KrtIrLoad(builder, result_var);
                }
                default: {
                    KrtIRValue void_val = {0};
                    void_val.type = KRT_IR_VALUE_VOID;
                    return void_val;
                }
            }
        }
        case AST_UNARY_OPERATION: {
            ASTNode* operand_node = expr->data.unary_op.operand;
            KrtIRValue operand_value = KrtIrGenerateExpression(builder, operand_node);
            switch (expr->data.unary_op.operator) {
                case TOKEN_MINUS: {
                    KrtIRValue zero = KrtIrImm(builder, 0);
                    return KrtIrSub(builder, zero, operand_value);
                }
                case TOKEN_NOT: {
                    KrtIRValue zero = KrtIrImm(builder, 0);
                    return KrtIrCompare(builder, KRT_IR_EQ, operand_value, zero);
                }
                case TOKEN_INCREMENT:
                case TOKEN_DECREMENT: {
                    if (!operand_node || operand_node->type != AST_IDENTIFIER) {
                        KrtIRValue void_val = {0};
                        void_val.type = KRT_IR_VALUE_VOID;
                        return void_val;
                    }
                    const char* target_name = operand_node->data.identifier_name;
                    KrtIRValue one = KrtIrImm(builder, 1);
                    KrtIRValue updated = (expr->data.unary_op.operator == TOKEN_INCREMENT)
                        ? KrtIrAdd(builder, operand_value, one)
                        : KrtIrSub(builder, operand_value, one);
                    KrtIrStore(builder, target_name, updated);
                    if (expr->data.unary_op.is_postfix) {
                        return operand_value;
                    }
                    return updated;
                }
                case TOKEN_INT32:
                case TOKEN_INT64:
                case TOKEN_INT16:
                case TOKEN_INT8:
                case TOKEN_UINT32:
                case TOKEN_UINT64:
                case TOKEN_UINT16:
                case TOKEN_UINT8:
                case TOKEN_FLOAT32:
                case TOKEN_FLOAT64:
                case TOKEN_BOOL:
                    return KrtIrCast(builder, operand_value, expr->data.unary_op.operator);
                default: {
                    KrtIRValue void_val = {0};
                    void_val.type = KRT_IR_VALUE_VOID;
                    return void_val;
                }
            }
        }
        case AST_ARRAY_ACCESS: {
            KrtIRValue array_expr = KrtIrGenerateExpression(builder, expr->data.array_access.array);
            KrtIRValue index_expr = KrtIrGenerateExpression(builder, expr->data.array_access.index);
            
            int element_size = 8; 
            
            KrtIRValue element_size_value = KrtIrImm(builder, element_size);
            KrtIRValue byte_offset = KrtIrMul(builder, index_expr, element_size_value);
            KrtIRValue base_addr;
            if (expr->data.array_access.array->type == AST_IDENTIFIER) {
                base_addr = KrtIrLoad(builder, expr->data.array_access.array->data.identifier_name);
            } else {
                base_addr = array_expr;
            }
            KrtIRValue element_addr = KrtIrAdd(builder, base_addr, byte_offset);
            KrtIRValue result = KrtIrLoadPtr(builder, element_addr, 0);
            return result;
        }
        case AST_STATIC_METHOD_CALL: {
            const char* class_name = expr->data.static_call.class_name;
            const char* method_name = expr->data.static_call.method_name;

            if (class_name && (strcmp(class_name, "Console") == 0 || strcmp(class_name, "console") == 0) && method_name) {
                bool is_print = false;
                bool has_newline = false;
                if (strcmp(method_name, "WriteLine") == 0 || strcmp(method_name, "writeLine") == 0 || 
                    strcmp(method_name, "writeline") == 0) {
                    is_print = true;
                    has_newline = true;
                } else if (strcmp(method_name, "Write") == 0 || strcmp(method_name, "write") == 0) {
                    is_print = true;
                    has_newline = false;
                }
                
                if (is_print) {
                    return KrtIrGeneratePrint(builder, expr->data.static_call.arguments, expr->data.static_call.argument_count, has_newline);
                }
            }

            if (method_name && strcmp(method_name, "delete") == 0) {
                char* dtor_name = KrtIrMangleStaticMember(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    KrtIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        KrtIrFunctionCreate(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    if (expr->data.static_call.argument_count > 0) {
                        KrtIRValue obj = KrtIrGenerateExpression(builder, expr->data.static_call.arguments[0]);
                        KrtIRValue* dargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                        if (dargs) {
                            dargs[0] = obj;
                            KrtIrCall(builder, dtor_name, dargs, 1);
                            KRT_FREE(dargs);
                        }
                        KrtIRValue* fargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                        if (fargs) {
                            fargs[0] = obj;
                            KrtIrCall(builder, "KRT_FREE", fargs, 1);
                            KRT_FREE(fargs);
                        }
                    }
                    KRT_FREE(dtor_name);
                }
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }
            
            int ns_count = 0;
            const char** ns_path = NULL;
            if (class_name) {
                ns_path = (const char**)KRT_MALLOC(2 * sizeof(const char*));
                if (ns_path) {
                    ns_path[0] = class_name;
                    ns_path[1] = NULL;
                    ns_count = 1;
                }
            }
            char* mangled_name = KrtIrMangleCallFunctionName(ns_path, method_name,
                                                               expr->data.static_call.arguments,
                                                               expr->data.static_call.argument_count);
            if (ns_path) {
                KRT_FREE(ns_path);
            }
            if (!mangled_name) {
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }
            bool function_exists = false;
            KrtIRFunction* iter = builder->module->functions;
            while (iter) {
                if (strcmp(iter->name, mangled_name) == 0) {
                    function_exists = true;
                    break;
                }
                iter = iter->next;
            }
            if (!function_exists) {
                KrtIrFunctionCreate(builder, mangled_name, NULL, -1, TOKEN_VOID);
            }

            KrtIRValue* args = NULL;
            if (expr->data.static_call.argument_count > 0) {
                args = (KrtIRValue*)KRT_MALLOC(expr->data.static_call.argument_count * sizeof(KrtIRValue));
                if (!args) {
                    KRT_FREE(mangled_name);
                    KrtIRValue void_val = {0};
                    void_val.type = KRT_IR_VALUE_VOID;
                    return void_val;
                }
                for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                    args[i] = KrtIrGenerateExpression(builder, expr->data.static_call.arguments[i]);
                }
            }

            KrtIRValue result = KrtIrCall(builder, mangled_name, args, expr->data.static_call.argument_count);

            if (args) {
                KRT_FREE(args);
            }
            KRT_FREE(mangled_name);
            return result;
        }

        case AST_MEMBER_ACCESS: {
            if (expr->data.member_access.object &&
                expr->data.member_access.object->type == AST_IDENTIFIER) {
                const char* class_name = expr->data.member_access.object->data.identifier_name;
                const char* member_name = expr->data.member_access.member_name;
                char* mangled_name = KrtIrMangleStaticMember(class_name, member_name);
                if (mangled_name) {
                    KrtIRGlobal* global = KrtIrModuleFindGlobal(builder->module, mangled_name);
                    if (global) {
                        KrtIRValue value = KrtIrLoad(builder, mangled_name);
                        KRT_FREE(mangled_name);
                        return value;
                    }
                    KRT_FREE(mangled_name);
                }
            }

            if (expr->data.member_access.object &&
                expr->data.member_access.object->type == AST_THIS) {
                const char* current_class = KrtIrCurrentClassContext(builder);
                const char* field_name = expr->data.member_access.member_name;
                int offset = KrtIrLayoutGetOffset(builder, current_class, field_name);
                KrtIRValue this_val = KrtIrArg(builder, 0);
                return KrtIrLoadPtr(builder, this_val, offset >= 0 ? offset : 0);
            }

            if (expr->data.member_access.resolved_class_name) {
                const char* field_name = expr->data.member_access.member_name;
                int offset = KrtIrLayoutGetOffset(builder, expr->data.member_access.resolved_class_name, field_name);
                KrtIRValue base = KrtIrGenerateExpression(builder, expr->data.member_access.object);
                return KrtIrLoadPtr(builder, base, offset >= 0 ? offset : 0);
            }

            KrtIRValue void_val = {0};
            void_val.type = KRT_IR_VALUE_VOID;
            return void_val;
        }

        case AST_CALL: {
            const char* func_name = expr->data.call.name;

            const char* alt_mangled = NULL;
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER && strcmp(expr->data.call.name, "delete") == 0) {
                const char* class_name = expr->data.call.object->data.identifier_name;
                char* dtor_name = KrtIrMangleStaticMember(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    KrtIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        KrtIrFunctionCreate(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    if (expr->data.call.argument_count > 0) {
                        KrtIRValue obj = KrtIrGenerateExpression(builder, expr->data.call.arguments[0]);
                        KrtIRValue* dargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                        if (dargs) {
                            dargs[0] = obj;
                            KrtIrCall(builder, dtor_name, dargs, 1);
                            KRT_FREE(dargs);
                        }
                        KrtIRValue* fargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                        if (fargs) {
                            fargs[0] = obj;
                            KrtIrCall(builder, "KRT_FREE", fargs, 1);
                            KRT_FREE(fargs);
                        }
                    }
                    KRT_FREE(dtor_name);
                }
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }
            if (expr->data.call.object && expr->data.call.object->type == AST_IDENTIFIER) {
                if (expr->data.call.resolved_class_name) {
                    
                    if (strstr(expr->data.call.name, "__") != NULL) {
                        
                        KrtIRValue* iargs = (KrtIRValue*)KRT_MALLOC(expr->data.call.argument_count * sizeof(KrtIRValue));
                        if (iargs) {
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                            }
                            KrtIRValue result = KrtIrCall(builder, expr->data.call.name, iargs, expr->data.call.argument_count);
                            KRT_FREE(iargs);
                            return result;
                        }
                    } else {
                        
                        const char* class_name = expr->data.call.resolved_class_name;
                        const char** ns_path = NULL;
                        if (class_name) {
                            ns_path = (const char**)KRT_MALLOC(2 * sizeof(const char*));
                            if (ns_path) {
                                ns_path[0] = class_name;
                                ns_path[1] = NULL;
                            }
                        }
                        char* mangled = KrtIrMangleCallFunctionName(ns_path, expr->data.call.name,
                                                                     expr->data.call.arguments,
                                                                     expr->data.call.argument_count);
                        if (ns_path) KRT_FREE(ns_path);
                        if (mangled) {
                            
                            KrtIRValue* iargs = (KrtIRValue*)KRT_MALLOC(expr->data.call.argument_count * sizeof(KrtIRValue));
                            if (iargs) {
                                for (int i = 0; i < expr->data.call.argument_count; i++) {
                                    iargs[i] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                                }
                                KrtIRValue result = KrtIrCall(builder, mangled, iargs, expr->data.call.argument_count);
                                KRT_FREE(iargs);
                                KRT_FREE(mangled);
                                return result;
                            }
                            KRT_FREE(mangled);
                        }
                    }
                } else {
                    const char* class_name = expr->data.call.object->data.identifier_name;
                    
                    if (strstr(func_name, "__") != NULL) {
                        
                    } else {
                        
                        const char** ns_path = NULL;
                        if (class_name) {
                            ns_path = (const char**)KRT_MALLOC(2 * sizeof(const char*));
                            if (ns_path) {
                                ns_path[0] = class_name;
                                ns_path[1] = NULL;
                            }
                        }
                        char* mangled = KrtIrMangleCallFunctionName(ns_path, func_name,
                                                                     expr->data.call.arguments,
                                                                     expr->data.call.argument_count);
                        if (ns_path) KRT_FREE(ns_path);
                        if (mangled) {
                            alt_mangled = mangled;
                            func_name = mangled;
                        }
                    }
                }
            } else if (expr->data.call.object && expr->data.call.object->type == AST_MEMBER_ACCESS) {
                
                const char** ns_path = NULL;
                int ns_count = 0;
                int ns_capacity = 8;
                ns_path = (const char**)KRT_MALLOC(sizeof(const char*) * (size_t)(ns_capacity + 1));
                if (!ns_path) {
                    ns_count = 0;
                } else {
                    
                    ASTNode* current = expr->data.call.object;
                    while (current && current->type == AST_MEMBER_ACCESS) {
                        if (ns_count >= ns_capacity) {
                            ns_capacity *= 2;
                            ns_path = (const char**)KRT_REALLOC(ns_path, sizeof(const char*) * (size_t)(ns_capacity + 1));
                            if (!ns_path) break;
                        }
                        ns_path[ns_count++] = current->data.member_access.member_name;
                        current = current->data.member_access.object;
                    }
                    if (current && current->type == AST_IDENTIFIER) {
                        if (ns_count >= ns_capacity) {
                            ns_capacity *= 2;
                            ns_path = (const char**)KRT_REALLOC(ns_path, sizeof(const char*) * (size_t)(ns_capacity + 1));
                        }
                        ns_path[ns_count++] = current->data.identifier_name;
                    }
                    
                    for (int i = 0; i < ns_count / 2; i++) {
                        const char* temp = ns_path[i];
                        ns_path[i] = ns_path[ns_count - 1 - i];
                        ns_path[ns_count - 1 - i] = temp;
                    }
                    ns_path[ns_count] = NULL;
                }
                
                char* mangled = NULL;
                KrtIRFunction* func_iter = builder->module->functions;
                while (func_iter) {
                    
                    const char* func_full_name = func_iter->name;
                    if (strncmp(func_full_name, "_ZN", 3) == 0) {
                        
                        char** demangled = NULL;
                        int demangled_count = 0;
                        if (name_demangle(func_full_name, &demangled, &demangled_count)) {
                            
                            if (demangled_count == ns_count + 1) {
                                bool match = true;
                                for (int i = 0; i < ns_count; i++) {
                                    if (strcmp(demangled[i], ns_path[i]) != 0) {
                                        match = false;
                                        break;
                                    }
                                }
                                if (match && strcmp(demangled[ns_count], expr->data.call.name) == 0) {
                                    
                                    mangled = KRT_STRDUP(func_full_name);
                                    name_demangle_free(demangled, demangled_count);
                                    break;
                                }
                            }
                            name_demangle_free(demangled, demangled_count);
                        }
                    }
                    func_iter = func_iter->next;
                }
                
                if (ns_path) {
                    KRT_FREE(ns_path);
                }
                
                if (mangled) {
                    alt_mangled = mangled;
                    func_name = mangled;
                }
            } else {
                
                int ns_count = 0;
                const char** ns_path = KrtIrGetNamespacePath(builder, &ns_count);
                if (ns_count > 0) {
                    
                    const char* base = expr->data.call.name ? expr->data.call.name : func_name;
                    char* mangled = KrtIrMangleCallFunctionName(ns_path, base,
                                                              expr->data.call.arguments,
                                                              expr->data.call.argument_count);
                    if (mangled) {
                        alt_mangled = mangled;
                        func_name = mangled;
                    }
                }
                if (ns_path) {
                    KRT_FREE(ns_path);
                }
                
                if (!alt_mangled) {
                    const char* current_class = KrtIrCurrentClassContext(builder);
                    if (current_class) {
                        char* mangled = KrtIrMangleStaticMember(current_class, func_name);
                        if (mangled) {
                            alt_mangled = mangled;
                            func_name = mangled;
                        }
                    }
                }
            }

            if (func_name) {
                bool has_newline = false;
                bool is_print = false;
                
                if (strcmp(func_name, "print") == 0 || strcmp(func_name, "println") == 0 ||
                    strcmp(func_name, "Console__WriteLine") == 0 ||
                    strcmp(func_name, "Console__WriteLineInt") == 0) {
                    is_print = true;
                    has_newline = true;
                } else if (strcmp(func_name, "Console__Write") == 0 ||
                           strcmp(func_name, "Console__WriteInt") == 0) {
                    is_print = true;
                    has_newline = false;
                }
                
                if (is_print) {
                    KrtIRValue result = KrtIrGeneratePrint(builder, expr->data.call.arguments, expr->data.call.argument_count, has_newline);
                    if (alt_mangled) KRT_FREE((char*)alt_mangled);
                    return result;
                }
            }

            bool function_exists = false;
            KrtIRFunction* func_iter = builder->module->functions;
            while (func_iter) {
                if (strcmp(func_iter->name, func_name) == 0) {
                    function_exists = true;
                    break;
                }
                func_iter = func_iter->next;
            }

            if (!function_exists) {
                
                int ns_count = 0;
                const char** ns_path = KrtIrGetNamespacePath(builder, &ns_count);
                const char* base_for_mangle = expr->data.call.name ? expr->data.call.name : func_name;
                char* mangled = KrtIrMangleCallFunctionName(ns_path, base_for_mangle,
                                                            expr->data.call.arguments,
                                                            expr->data.call.argument_count);
                if (ns_path) {
                    KRT_FREE(ns_path);
                }
                
                if (mangled) {
                    func_iter = builder->module->functions;
                    while (func_iter) {
                        if (strcmp(func_iter->name, mangled) == 0) {
                            function_exists = true;
                            func_name = mangled;  
                            break;
                        }
                        func_iter = func_iter->next;
                    }
                    if (!function_exists) {
                        KRT_FREE((char*)mangled);
                    }
                }
            }
            
            bool is_runtime_func = 
                strcmp(func_name, "timer_start") == 0 || 
                strcmp(func_name, "timer_elapsed") == 0 ||
                strcmp(func_name, "timer_current") == 0 ||
                strcmp(func_name, "timer_start_int") == 0 ||
                strcmp(func_name, "timer_elapsed_int") == 0 ||
                strcmp(func_name, "timer_current_int") == 0 ||
                strcmp(func_name, "print") == 0 ||
                strcmp(func_name, "println") == 0 ||
                strncmp(func_name, "Console__", 9) == 0 ||
                strncmp(func_name, "_es_", 4) == 0;

            if (!function_exists && !is_runtime_func) {
                
                char lambda_name[256];
                snprintf(lambda_name, sizeof(lambda_name), "__lambda_%s", func_name);
                
                KrtIRFunction* lambda_func = builder->module->functions;
                bool lambda_found = false;
                while (lambda_func) {
                    if (strncmp(lambda_func->name, "__lambda_", 9) == 0) {
                        lambda_found = true;
                        break;
                    }
                    lambda_func = lambda_func->next;
                }
                
                if (!lambda_found) {
                    
                }
            }

            KrtIRValue* args = NULL;
            if (expr->data.call.argument_count > 0) {
                args = (KrtIRValue*)KRT_MALLOC(expr->data.call.argument_count * sizeof(KrtIRValue));
                if (!args) {
                    KrtIRValue void_val = {0};
                    void_val.type = KRT_IR_VALUE_VOID;
                    return void_val;
                }
                for (int i = 0; i < expr->data.call.argument_count; i++) {
                    args[i] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                }
            }

            if (expr->data.call.object && expr->data.call.object->type == AST_THIS) {
                const char* current_class = KrtIrCurrentClassContext(builder);
                if (current_class) {
                    char* mangled = KrtIrMangleStaticMember(current_class, expr->data.call.name);
                    if (mangled) {
                        KrtIRValue* iargs = (KrtIRValue*)KRT_MALLOC((expr->data.call.argument_count + 1) * sizeof(KrtIRValue));
                        if (iargs) {
                            iargs[0] = KrtIrArg(builder, 0);
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i + 1] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                            }
                            KrtIRValue result = KrtIrCall(builder, mangled, iargs, expr->data.call.argument_count + 1);
                            KRT_FREE(iargs);
                            KRT_FREE(mangled);
                            if (alt_mangled) {
                                KRT_FREE((char*)alt_mangled);
                            }
                            return result;
                        }
                        KRT_FREE(mangled);
                    }
                }
            }

            if (expr->data.call.resolved_class_name && expr->data.call.object) {
                
                if (strstr(expr->data.call.name, "__") != NULL) {
                    
                    KrtIRValue* iargs = (KrtIRValue*)KRT_MALLOC(expr->data.call.argument_count * sizeof(KrtIRValue));
                    if (iargs) {
                        for (int i = 0; i < expr->data.call.argument_count; i++) {
                            iargs[i] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                        }
                        KrtIRValue result = KrtIrCall(builder, expr->data.call.name, iargs, expr->data.call.argument_count);
                        KRT_FREE(iargs);
                        return result;
                    }
                } else {
                    char* mangled = KrtIrMangleStaticMember(expr->data.call.resolved_class_name, expr->data.call.name);
                    if (mangled) {
                        
                        KrtIRValue* iargs = (KrtIRValue*)KRT_MALLOC(expr->data.call.argument_count * sizeof(KrtIRValue));
                        if (iargs) {
                            for (int i = 0; i < expr->data.call.argument_count; i++) {
                                iargs[i] = KrtIrGenerateExpression(builder, expr->data.call.arguments[i]);
                            }
                            KrtIRValue result = KrtIrCall(builder, mangled, iargs, expr->data.call.argument_count);
                            KRT_FREE(iargs);
                            KRT_FREE(mangled);
                            return result;
                        }
                        KRT_FREE(mangled);
                    }
                }
            }

            const char* call_func_name = func_name;
            char* call_mangled = NULL;
            if (strncmp(func_name, "_ZN", 3) != 0) {
                
                int ns_count = 0;
                const char** ns_path = KrtIrGetNamespacePath(builder, &ns_count);
                call_mangled = KrtIrMangleCallFunctionName(ns_path, func_name,
                                                            expr->data.call.arguments,
                                                            expr->data.call.argument_count);
                if (ns_path) {
                    KRT_FREE(ns_path);
                }
                if (call_mangled) {
                    call_func_name = call_mangled;
                }
            }
            
            KrtIRValue result = KrtIrCall(builder, call_func_name, args, expr->data.call.argument_count);
            
            if (call_mangled) {
                KRT_FREE(call_mangled);
            }
            if (alt_mangled) {
                KRT_FREE((char*)alt_mangled);
            }
            if (args) {
                KRT_FREE(args);
            }
            return result;
        }

        case AST_TERNARY_OPERATION: {
            KrtIRValue cond = KrtIrGenerateExpression(builder, expr->data.ternary_op.condition);

            char true_label[32], false_label[32], end_label[32];
            int true_label_id = builder->label_counter++;
            int false_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;

            snprintf(true_label, sizeof(true_label), "if_true_%d", true_label_id);
            snprintf(false_label, sizeof(false_label), "if_false_%d", false_label_id);
            snprintf(end_label, sizeof(end_label), "if_end_%d", end_label_id);

            char result_var[32];
            snprintf(result_var, sizeof(result_var), "result_%d", builder->temp_counter++);

            KrtIRBasicBlock* true_block = KrtIrBlockCreate(builder, true_label);
            KrtIRBasicBlock* false_block = KrtIrBlockCreate(builder, false_label);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
            
            KrtIRBasicBlock* saved_current_block = builder->current_block;

            KrtIrBranch(builder, cond, true_block, false_block);

            if (saved_current_block) {
                saved_current_block->next = true_block;
            }
            true_block->next = false_block;
            false_block->next = end_block;

            KrtIrBlockSetCurrent(builder, true_block);
            KrtIRValue true_value = KrtIrGenerateExpression(builder, expr->data.ternary_op.true_value);
            KrtIrStore(builder, result_var, true_value);
            KrtIrJump(builder, end_block);

            KrtIrBlockSetCurrent(builder, false_block);
            KrtIRValue false_value = KrtIrGenerateExpression(builder, expr->data.ternary_op.false_value);
            KrtIrStore(builder, result_var, false_value);
            KrtIrJump(builder, end_block);

            KrtIrBlockSetCurrent(builder, end_block);

            return KrtIrLoad(builder, result_var);
        }

        case AST_ARRAY_LITERAL: {
            int element_count = expr->data.array_literal.element_count;

            if (element_count == 0) {
                KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                if (args) {
                    args[0] = KrtIrImm(builder, 8);
                    KrtIRValue array_ptr = KrtIrCall(builder, "KrtMalloc", args, 1);
                    KRT_FREE(args);
                    return array_ptr;
                }
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }

            int element_size = 8; 
            
            KrtIRValue element_size_value = KrtIrImm(builder, element_size);
            KrtIRValue count_val = KrtIrImm(builder, element_count);
            KrtIRValue total_size = KrtIrMul(builder, count_val, element_size_value);

            KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
            if (args) {
                args[0] = total_size;
                KrtIRValue array_ptr = KrtIrCall(builder, "KrtMalloc", args, 1);
                KRT_FREE(args);

                for (int i = 0; i < element_count; i++) {
                    KrtIRValue element_val = KrtIrGenerateExpression(builder, expr->data.array_literal.elements[i]);
                    KrtIRValue index_val = KrtIrImm(builder, i);
                    KrtIrArrayStore(builder, array_ptr, index_val, element_val);
                }

                return array_ptr;
            }
            KrtIRValue void_val = {0};
            void_val.type = KRT_IR_VALUE_VOID;
            return void_val;
        }

        case AST_LAMBDA_EXPRESSION: {
            
            static int lambda_counter = 0;
            char lambda_name[256];
            snprintf(lambda_name, sizeof(lambda_name), "__lambda_%d", lambda_counter++);

            int param_count = expr->data.lambda_expr.parameter_count;

            KrtIRParam* params = NULL;
            if (param_count > 0) {
                params = (KrtIRParam*)KRT_MALLOC(param_count * sizeof(KrtIRParam));
                if (!params) {
                    KrtIRValue void_val = {0};
                    void_val.type = KRT_IR_VALUE_VOID;
                    return void_val;
                }
                for (int i = 0; i < param_count; i++) {
                    params[i].name = KRT_STRDUP(expr->data.lambda_expr.parameters[i]);
                    params[i].type = TOKEN_INT32; 
                }
            }

            KrtIRFunction* lambda_func = KrtIrFunctionCreate(builder, lambda_name,
                params, param_count, TOKEN_INT32); 

            if (params) {
                for (int i = 0; i < param_count; i++) {
                    KRT_FREE(params[i].name);
                }
                KRT_FREE(params);
            }

            if (!lambda_func) {
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            }

            KrtIRFunction* saved_func = builder->current_function;
            KrtIRBasicBlock* saved_block = builder->current_block;

            builder->current_function = lambda_func;
            KrtIRBasicBlock* lambda_entry = KrtIrBlockCreate(builder, "entry");
            KrtIrBlockSetCurrent(builder, lambda_entry);

            if (expr->data.lambda_expr.expression) {
                
                KrtIRValue result = KrtIrGenerateExpression(builder, expr->data.lambda_expr.expression);
                KrtIrReturn(builder, result);
            } else if (expr->data.lambda_expr.body) {
                
                KrtIrGenerateStatement(builder, expr->data.lambda_expr.body);
                
                if (!KrtIrCheckHasReturn(expr->data.lambda_expr.body)) {
                    KrtIRValue zero = KrtIrImm(builder, 0);
                    KrtIrReturn(builder, zero);
                }
            }

            builder->current_function = saved_func;
            builder->current_block = saved_block;

            KrtIRValue lambda_val = KrtIrImm(builder, 0);

            return lambda_val;
        }

        case AST_LINQ_QUERY: {
            KrtIRValue void_val = {0};
            void_val.type = KRT_IR_VALUE_VOID;
            return void_val;
        }

        case AST_UNSAFE_CALL: {
            ASTNode* nested_node = expr->data.unsafe_call.expression;
            
            if (expr->data.unsafe_call.is_block) {
                KrtIrGenerateStatement(builder, nested_node);
                KrtIRValue void_val = {0};
                void_val.type = KRT_IR_VALUE_VOID;
                return void_val;
            } else {
                KrtIRValue result = KrtIrGenerateExpression(builder, nested_node);
                return result;
            }
        }

        default: {
            KrtIRValue void_val = {0};
            void_val.type = KRT_IR_VALUE_VOID;
            return void_val;
        }
    }
}

static void KrtIrGenerateStatement(KrtIRBuilder* builder, ASTNode* stmt) {
    if (!stmt) {
        return;
    }

    switch (stmt->type) {
        case AST_ASSIGNMENT: {
            KrtIRValue value = KrtIrGenerateExpression(builder, stmt->data.assignment.value);
            const char* target_name = stmt->data.assignment.name;
            const char* current_class = KrtIrCurrentClassContext(builder);
            int stored = 0;
            if (current_class && target_name) {
                char* mangled = KrtIrMangleStaticMember(current_class, target_name);
                if (mangled) {
                    KrtIRGlobal* global = KrtIrModuleFindGlobal(builder->module, mangled);
                    if (global) {
                        KrtIrStore(builder, mangled, value);
                        stored = 1;
                    }
                    KRT_FREE(mangled);
                }
            }
            if (!stored) {
                KrtIrStore(builder, target_name, value);
            }
            break;
        }

        case AST_COMPOUND_ASSIGNMENT: {
            const char* target_name = stmt->data.compound_assignment.name;
            KrtIRValue current_val = KrtIrLoad(builder, target_name);
            KrtIRValue rhs = KrtIrGenerateExpression(builder, stmt->data.compound_assignment.value);
            KrtIRValue result;
            
            switch (stmt->data.compound_assignment.operator) {
                case TOKEN_PLUS_ASSIGN: result = KrtIrAdd(builder, current_val, rhs); break;
                case TOKEN_MINUS_ASSIGN: result = KrtIrSub(builder, current_val, rhs); break;
                case TOKEN_MUL_ASSIGN: result = KrtIrMul(builder, current_val, rhs); break;
                case TOKEN_DIV_ASSIGN: result = KrtIrDiv(builder, current_val, rhs); break;
                default: result = rhs; break;
            }
            
            KrtIrStore(builder, target_name, result);
            break;
        }

        case AST_ARRAY_ASSIGNMENT: {
            KrtIRValue array_value = KrtIrGenerateExpression(builder, stmt->data.array_assignment.array);
            KrtIRValue index_value = KrtIrGenerateExpression(builder, stmt->data.array_assignment.index);
            KrtIRValue value = KrtIrGenerateExpression(builder, stmt->data.array_assignment.value);
            KrtIrArrayStore(builder, array_value, index_value, value);
            break;
        }

        case AST_ARRAY_COMPOUND_ASSIGNMENT: {
            KrtIRValue array_value = KrtIrGenerateExpression(builder, stmt->data.array_compound_assignment.array);
            KrtIRValue index_value = KrtIrGenerateExpression(builder, stmt->data.array_compound_assignment.index);
            
            int element_size = 8;
            KrtIRValue element_size_value = KrtIrImm(builder, element_size);
            KrtIRValue byte_offset = KrtIrMul(builder, index_value, element_size_value);
            KrtIRValue element_addr = KrtIrAdd(builder, array_value, byte_offset);
            KrtIRValue current_val = KrtIrLoadPtr(builder, element_addr, 0);
            
            KrtIRValue rhs = KrtIrGenerateExpression(builder, stmt->data.array_compound_assignment.value);
            KrtIRValue result;
            
            switch (stmt->data.array_compound_assignment.operator) {
                case TOKEN_PLUS_ASSIGN: result = KrtIrAdd(builder, current_val, rhs); break;
                case TOKEN_MINUS_ASSIGN: result = KrtIrSub(builder, current_val, rhs); break;
                case TOKEN_MUL_ASSIGN: result = KrtIrMul(builder, current_val, rhs); break;
                case TOKEN_DIV_ASSIGN: result = KrtIrDiv(builder, current_val, rhs); break;
                default: result = rhs; break;
            }
            
            KrtIrArrayStore(builder, array_value, index_value, result);
            break;
        }

        case AST_RETURN_STATEMENT: {
            KrtIRValue value = KrtIrGenerateExpression(builder, stmt->data.return_stmt.value);
            KrtIrReturn(builder, value);
            break;
        }

        case AST_IF_STATEMENT: {

            KrtIRValue cond = KrtIrGenerateExpression(builder, stmt->data.if_stmt.condition);

            char true_label[32], false_label[32], end_label[32];
            int true_label_id = builder->label_counter++;
            int false_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;

            snprintf(true_label, sizeof(true_label), "if_true_%d", true_label_id);
            snprintf(false_label, sizeof(false_label), "if_false_%d", false_label_id);
            snprintf(end_label, sizeof(end_label), "if_end_%d", end_label_id);

            KrtIRBasicBlock* true_block = KrtIrBlockCreate(builder, true_label);
            KrtIRBasicBlock* false_block = KrtIrBlockCreate(builder, false_label);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);

            KrtIrBranch(builder, cond, true_block, false_block);

            KrtIrBlockSetCurrent(builder, true_block);
            KrtIrGenerateStatement(builder, stmt->data.if_stmt.then_branch);

            int then_has_return = KrtIrCheckHasReturn(stmt->data.if_stmt.then_branch);
            if (!then_has_return) {
                KrtIrJump(builder, end_block);
            }

            KrtIrBlockSetCurrent(builder, false_block);
            if (stmt->data.if_stmt.else_branch) {
                KrtIrGenerateStatement(builder, stmt->data.if_stmt.else_branch);

                int else_has_return = KrtIrCheckHasReturn(stmt->data.if_stmt.else_branch);
                if (!else_has_return) {
                    KrtIrJump(builder, end_block);
                }
            } else {
                KrtIrJump(builder, end_block);
            }

            KrtIrBlockSetCurrent(builder, end_block);
            break;
        }

        case AST_WHILE_STATEMENT: {
            char cond_label[32], body_label[32], end_label[32];
            int cond_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(cond_label, sizeof(cond_label), "while_cond_%d", cond_label_id);
            snprintf(body_label, sizeof(body_label), "while_body_%d", body_label_id);
            snprintf(end_label, sizeof(end_label), "while_end_%d", end_label_id);
            KrtIRBasicBlock* cond_block = KrtIrBlockCreate(builder, cond_label);
            KrtIRBasicBlock* body_block = KrtIrBlockCreate(builder, body_label);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
            KrtIrPushLoopContext(builder, cond_block, end_block);
            KrtIrJump(builder, cond_block);
            KrtIrBlockSetCurrent(builder, cond_block);
            KrtIRValue cond = KrtIrGenerateExpression(builder, stmt->data.while_stmt.condition);
            KrtIrBranch(builder, cond, body_block, end_block);
            KrtIrBlockSetCurrent(builder, body_block);
            KrtIrGenerateStatement(builder, stmt->data.while_stmt.body);
            KrtIrJump(builder, cond_block);
            KrtIrBlockSetCurrent(builder, end_block);
            KrtIrPopLoopContext(builder);
            break;
        }

        case AST_FOR_STATEMENT: {
            if (stmt->data.for_stmt.init) {
                if (stmt->data.for_stmt.init->type == AST_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_ARRAY_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_ARRAY_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.init->type == AST_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.init->type == AST_STATIC_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.init->type == AST_BLOCK) {
                    KrtIrGenerateStatement(builder, stmt->data.for_stmt.init);
                } else {
                    KrtIrGenerateExpression(builder, stmt->data.for_stmt.init);
                }
            }
            char cond_label[32], body_label[32], incr_label[32], end_label[32];
            int cond_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int incr_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(cond_label, sizeof(cond_label), "for_cond_%d", cond_label_id);
            snprintf(body_label, sizeof(body_label), "for_body_%d", body_label_id);
            snprintf(incr_label, sizeof(incr_label), "for_incr_%d", incr_label_id);
            snprintf(end_label, sizeof(end_label), "for_end_%d", end_label_id);
            KrtIRBasicBlock* cond_block = KrtIrBlockCreate(builder, cond_label);
            KrtIRBasicBlock* body_block = KrtIrBlockCreate(builder, body_label);
            KrtIRBasicBlock* incr_block = KrtIrBlockCreate(builder, incr_label);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
            KrtIrPushLoopContext(builder, incr_block, end_block);
            KrtIrJump(builder, cond_block);
            KrtIrBlockSetCurrent(builder, cond_block);
            KrtIRValue cond_value;
            if (stmt->data.for_stmt.condition) {
                cond_value = KrtIrGenerateExpression(builder, stmt->data.for_stmt.condition);
                if (cond_value.type == KRT_IR_VALUE_VOID) {
                    cond_value = KrtIrImm(builder, 1);
                }
            } else {
                cond_value = KrtIrImm(builder, 1);
            }
            KrtIrBranch(builder, cond_value, body_block, end_block);
            KrtIrBlockSetCurrent(builder, body_block);
            KrtIrGenerateStatement(builder, stmt->data.for_stmt.body);
            KrtIrJump(builder, incr_block);
            KrtIrBlockSetCurrent(builder, incr_block);
            if (stmt->data.for_stmt.increment) {
                if (stmt->data.for_stmt.increment->type == AST_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_ARRAY_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_ARRAY_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_STATIC_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_BLOCK) {
                    KrtIrGenerateStatement(builder, stmt->data.for_stmt.increment);
                } else {
                    KrtIrGenerateExpression(builder, stmt->data.for_stmt.increment);
                }
            }
            KrtIrJump(builder, cond_block);
            KrtIrBlockSetCurrent(builder, end_block);
            KrtIrPopLoopContext(builder);
            break;
        }
        case AST_FOREACH_STATEMENT: {
            char* var_name = stmt->data.foreach_stmt.var_name;
            ASTNode* iterable = stmt->data.foreach_stmt.iterable;
            ASTNode* body = stmt->data.foreach_stmt.body;
            
            KrtIRValue iterable_value = KrtIrGenerateExpression(builder, iterable);
            
            char index_var_name[256];
            snprintf(index_var_name, sizeof(index_var_name), "%s__idx", var_name);
            KrtIrAlloc(builder, index_var_name);
            KrtIrStore(builder, index_var_name, KrtIrImm(builder, 0));
            
            char loop_label[32], body_label[32], end_label[32];
            int loop_label_id = builder->label_counter++;
            int body_label_id = builder->label_counter++;
            int end_label_id = builder->label_counter++;
            snprintf(loop_label, sizeof(loop_label), "foreach_cond_%d", loop_label_id);
            snprintf(body_label, sizeof(body_label), "foreach_body_%d", body_label_id);
            snprintf(end_label, sizeof(end_label), "foreach_end_%d", end_label_id);
            
            KrtIRBasicBlock* loop_block = KrtIrBlockCreate(builder, loop_label);
            KrtIRBasicBlock* body_block = KrtIrBlockCreate(builder, body_label);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
            
            KrtIrPushLoopContext(builder, end_block, end_block);
            KrtIrJump(builder, loop_block);
            
            KrtIrBlockSetCurrent(builder, loop_block);
            
            KrtIRValue index_value = KrtIrLoad(builder, index_var_name);
            KrtIRValue array_len = KrtIrCall(builder, "array_size", &iterable_value, 1);
            KrtIRValue cond = KrtIrCompare(builder, KRT_IR_LT, index_value, array_len);
            KrtIrBranch(builder, cond, body_block, end_block);
            
            KrtIrBlockSetCurrent(builder, body_block);
            
            KrtIRValue current_index = KrtIrLoad(builder, index_var_name);
            KrtIRValue* array_args = (KrtIRValue*)KRT_MALLOC(2 * sizeof(KrtIRValue));
            array_args[0] = iterable_value;
            array_args[1] = current_index;
            KrtIRValue element_ptr = KrtIrCall(builder, "array_get", array_args, 2);
            KRT_FREE(array_args);
            KrtIRValue element_value = KrtIrLoadPtr(builder, element_ptr, 0);
            
            KrtIrAlloc(builder, var_name);
            KrtIrStore(builder, var_name, element_value);
            
            TypeCheckSymbolTable* foreach_scope = NULL;
            TypeCheckSymbolTable* saved_scope = NULL;
            if (builder->type_context) {
                saved_scope = builder->type_context->current_scope;
                foreach_scope = type_check_symbol_table_create(saved_scope);
                builder->type_context->current_scope = foreach_scope;
                
                TypeCheckSymbol symbol = {0};
                symbol.name = var_name;
                symbol.type = type_create_basic(TYPE_INT64);
                symbol.is_constant = 0;
                symbol.line_number = 0;
                symbol.owns_name = 0;
                type_check_symbol_table_add(builder->type_context->current_scope, symbol);
            }
            
            KrtIrGenerateStatement(builder, body);
            
            KrtIRValue new_index = KrtIrAdd(builder, KrtIrLoad(builder, index_var_name), KrtIrImm(builder, 1));
            KrtIrStore(builder, index_var_name, new_index);
            
            KrtIrJump(builder, loop_block);
            
            KrtIrBlockSetCurrent(builder, end_block);
            
            if (builder->type_context) {
                builder->type_context->current_scope = saved_scope;
                type_check_symbol_table_unref(foreach_scope);
            }
            
            KrtIrPopLoopContext(builder);
            break;
        }
        case AST_PRINT_STATEMENT: {
            KrtIrGeneratePrint(builder, stmt->data.print_stmt.values, stmt->data.print_stmt.value_count, stmt->data.print_stmt.has_newline);
            break;
        }

        case AST_BLOCK: {
            KrtIrGenerateBlock(builder, stmt);
            break;
        }

        case AST_SWITCH_STATEMENT: {
            KrtIRValue cond = KrtIrGenerateExpression(builder, stmt->data.switch_stmt.expression);
            char end_label[32];
            int end_label_id = builder->label_counter++;
            snprintf(end_label, sizeof(end_label), "end_%d", end_label_id);
            KrtIRBasicBlock* end_block = KrtIrBlockCreate(builder, end_label);
            KrtIrPushLoopContext(builder, NULL, end_block);
            char** case_labels = (char**)KRT_MALLOC(stmt->data.switch_stmt.case_count * sizeof(char*));
            KrtIRBasicBlock** case_blocks = (KrtIRBasicBlock**)KRT_MALLOC(stmt->data.switch_stmt.case_count * sizeof(KrtIRBasicBlock*));
            if (case_labels && case_blocks) {
                for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                    case_labels[i] = (char*)KRT_MALLOC(32);
                    if (case_labels[i]) {
                        int case_label_id = builder->label_counter++;
                        snprintf(case_labels[i], 32, "case_%d_%d", case_label_id, i + 1);
                        case_blocks[i] = KrtIrBlockCreate(builder, case_labels[i]);
                    }
                }
            }
            KrtIRBasicBlock* default_block = NULL;
            char* default_label = NULL;
            if (stmt->data.switch_stmt.default_case) {
                default_label = (char*)KRT_MALLOC(32);
                if (default_label) {
                    int default_label_id = builder->label_counter++;
                    snprintf(default_label, 32, "default_%d", default_label_id);
                    default_block = KrtIrBlockCreate(builder, default_label);
                }
            }
            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                KrtIRValue case_value = KrtIrGenerateExpression(builder, stmt->data.switch_stmt.cases[i]->data.case_clause.value);
                KrtIRValue cmp_result = KrtIrCompare(builder, KRT_IR_EQ, cond, case_value);
                KrtIrBranch(builder, cmp_result, case_blocks[i],
                            (i < stmt->data.switch_stmt.case_count - 1) ? case_blocks[i + 1] :
                            (default_block ? default_block : end_block));
            }
            if (stmt->data.switch_stmt.default_case) {
                KrtIrJump(builder, default_block);
            } else if (stmt->data.switch_stmt.case_count > 0) {
                KrtIrJump(builder, end_block);
            }
            for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                KrtIrBlockSetCurrent(builder, case_blocks[i]);
                KrtIrGenerateStatement(builder, stmt->data.switch_stmt.cases[i]);
                KrtIrJump(builder, end_block);
            }
            if (stmt->data.switch_stmt.default_case) {
                KrtIrBlockSetCurrent(builder, default_block);
                KrtIrGenerateStatement(builder, stmt->data.switch_stmt.default_case);
                KrtIrJump(builder, end_block);
            }
            KrtIrBlockSetCurrent(builder, end_block);
            KrtIrNop(builder);
            KrtIrPopLoopContext(builder);
            if (case_labels) {
                for (int i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                    if (case_labels[i]) {
                        KRT_FREE(case_labels[i]);
                    }
                }
                KRT_FREE(case_labels);
            }
            if (case_blocks) {
                KRT_FREE(case_blocks);
            }
            if (default_label) {
                KRT_FREE(default_label);
            }
            break;
        }
        case AST_CASE_CLAUSE: {
            for (int i = 0; i < stmt->data.case_clause.statement_count; i++) {
                KrtIrGenerateStatement(builder, stmt->data.case_clause.statements[i]);
            }
            break;
        }
        case AST_DEFAULT_CLAUSE: {
            for (int i = 0; i < stmt->data.default_clause.statement_count; i++) {
                KrtIrGenerateStatement(builder, stmt->data.default_clause.statements[i]);
            }
            break;
        }
        case AST_BREAK_STATEMENT: {
            KrtIRBasicBlock* break_block = KrtIrGetCurrentBreakBlock(builder);
            if (break_block) {
                KrtIrJump(builder, break_block);
            }
            break;
        }
        case AST_CONTINUE_STATEMENT: {
            KrtIRBasicBlock* continue_block = KrtIrGetCurrentContinueBlock(builder);
            if (continue_block) {
                KrtIrJump(builder, continue_block);
            }
            break;
        }
        case AST_DELETE_STATEMENT: {
            KrtIRValue obj = KrtIrGenerateExpression(builder, stmt->data.delete_stmt.value);
            const char* class_name = stmt->data.delete_stmt.resolved_class_name;
            if (class_name) {
                char* dtor_name = KrtIrMangleStaticMember(class_name, "destructor");
                if (dtor_name) {
                    bool exists = false;
                    KrtIRFunction* f = builder->module->functions;
                    while (f) {
                        if (strcmp(f->name, dtor_name) == 0) {
                            exists = true;
                            break;
                        }
                        f = f->next;
                    }
                    if (!exists) {
                        KrtIrFunctionCreate(builder, dtor_name, NULL, -1, TOKEN_VOID);
                    }
                    KrtIRValue* dargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                    if (dargs) {
                        dargs[0] = obj;
                        KrtIrCall(builder, dtor_name, dargs, 1);
                        KRT_FREE(dargs);
                    }
                    KrtIRValue* fargs = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                    if (fargs) {
                        fargs[0] = obj;
                        KrtIrCall(builder, "KRT_FREE", fargs, 1);
                        KRT_FREE(fargs);
                    }
                    KRT_FREE(dtor_name);
                }
            }
            break;
        }

        case AST_UNSAFE_CALL: {
            KrtIrGenerateExpression(builder, stmt);
            break;
        }

        case AST_VARIABLE_DECLARATION: {
            if (builder->type_context && builder->type_context->current_scope) {
                TypeCheckSymbol symbol = {0};
                symbol.name = (char*)stmt->data.variable_decl.name;
                symbol.type = type_create_from_token(stmt->data.variable_decl.type);
                symbol.is_constant = 0;
                symbol.line_number = 0;
                symbol.owns_name = 0;
                if (symbol.type) {
                    type_check_symbol_table_add(builder->type_context->current_scope, symbol);
                }
            }
            bool is_c_style_array = (stmt->data.variable_decl.array_size != NULL);
            bool is_array_literal = false;
            if (stmt->data.variable_decl.value &&
                stmt->data.variable_decl.value->type == AST_ARRAY_LITERAL) {
                is_array_literal = true;
            }
            if (is_c_style_array) {
                int array_size = 10;
                if (stmt->data.variable_decl.array_size) {
                    double size_value;
                    if (try_extract_constant(stmt->data.variable_decl.array_size, &size_value)) {
                        array_size = (int)size_value;
                        if (array_size <= 0) {
                            array_size = 10;
                        }
                    }
                }
                
                int element_size = 8; 
                
                int total_bytes = array_size * element_size;
                KrtIrAlloc(builder, stmt->data.variable_decl.name);
                KrtIRValue* args = (KrtIRValue*)KRT_MALLOC(sizeof(KrtIRValue));
                if (args) {
                    args[0] = KrtIrImm(builder, total_bytes);
                    KrtIRValue array_ptr = KrtIrCall(builder, "KrtMalloc", args, 1);
                    KRT_FREE(args);
                    KrtIrStore(builder, stmt->data.variable_decl.name, array_ptr);
                }
            } else if (is_array_literal) {
                KrtIrAlloc(builder, stmt->data.variable_decl.name);
                if (stmt->data.variable_decl.value) {
                    KrtIRValue init_value = KrtIrGenerateExpression(builder, stmt->data.variable_decl.value);
                    KrtIrStore(builder, stmt->data.variable_decl.name, init_value);
                }
            } else {
                KrtIrAlloc(builder, stmt->data.variable_decl.name);
                if (stmt->data.variable_decl.value) {
                    KrtIRValue init_value = KrtIrGenerateExpression(builder, stmt->data.variable_decl.value);
                    KrtIrStore(builder, stmt->data.variable_decl.name, init_value);
                }
            }
            break;
        }
        case AST_STATIC_VARIABLE_DECLARATION: {
            const char* base_name = stmt->data.static_variable_decl.name;
            if (!base_name) {
                break;
            }
            const char* current_class = KrtIrCurrentClassContext(builder);
            char* mangled_name = NULL;
            const char* global_name = base_name;
            if (current_class) {
                mangled_name = KrtIrMangleStaticMember(current_class, base_name);
                if (mangled_name) {
                    global_name = mangled_name;
                }
            }
            KrtTokenType declared_type = stmt->data.static_variable_decl.type;
            if (declared_type == TOKEN_UNKNOWN) {
                declared_type = TOKEN_INT32;
            }
            KrtIRGlobal* global = KrtIrModuleAddGlobal(builder, global_name, declared_type);
            if (!global) {
                if (mangled_name) {
                    KRT_FREE(mangled_name);
                }
                break;
            }
            if (stmt->data.static_variable_decl.value) {
                double init_value = 0;
                if (KrtIrEvaluateNumericConstant(stmt->data.static_variable_decl.value, &init_value)) {
                    KrtIrModuleSetGlobalNumberInitializer(global, init_value);
                } else {
                    KrtIRFunction* saved_function = builder->current_function;
                    KrtIRBasicBlock* saved_block = builder->current_block;
                    KrtIrEnsureMainEntry(builder);
                    KrtIRValue value = KrtIrGenerateExpression(builder, stmt->data.static_variable_decl.value);
                    KrtIrStore(builder, global_name, value);
                    builder->current_function = saved_function;
                    builder->current_block = saved_block;
                }
            }
            if (mangled_name) {
                KRT_FREE(mangled_name);
            }
            break;
        }
        case AST_FUNCTION_DECLARATION: {
            
            if (!stmt->data.function_decl.body) {
                
                break;
            }
            
            KrtIRParam* params = NULL;
            if (stmt->data.function_decl.parameter_count > 0) {
                params = (KrtIRParam*)KRT_MALLOC(stmt->data.function_decl.parameter_count * sizeof(KrtIRParam));
                if (params) {
                    for (int i = 0; i < stmt->data.function_decl.parameter_count; i++) {
                        params[i].name = stmt->data.function_decl.parameters[i];
                        params[i].type = stmt->data.function_decl.parameter_types[i];
                    }
                }
            }
            
            int ns_count = 0;
            const char** ns_path = KrtIrGetNamespacePath(builder, &ns_count);
            char* mangled_name = name_mangle_function(ns_path, stmt->data.function_decl.name, 
                                                       stmt->data.function_decl.parameter_types,
                                                       stmt->data.function_decl.parameter_count);
            if (ns_path) {
                KRT_FREE(ns_path);
            }
            
            const char* func_name = mangled_name ? mangled_name : stmt->data.function_decl.name;
            KrtIRFunction* func = KrtIrFunctionCreate(builder, func_name, params, stmt->data.function_decl.parameter_count, stmt->data.function_decl.return_type);
            KrtIRFunction* saved_function = builder->current_function;
            builder->current_function = func;
            TypeCheckSymbolTable* saved_scope = NULL;
            if (builder->type_context) {
                saved_scope = builder->type_context->current_scope;
                TypeCheckSymbol* func_symbol = type_check_symbol_table_lookup(saved_scope, stmt->data.function_decl.name);
                if (func_symbol && func_symbol->type && func_symbol->type->kind == TYPE_FUNCTION && func_symbol->type->data.function.function_scope) {
                    builder->type_context->current_scope = func_symbol->type->data.function.function_scope;
                }
            }
            
            KrtIrFunctionSetEntry(builder, func);
            if (params) {
                KRT_FREE(params);
            }
            
            KrtIRBasicBlock* entry_block = func->entry_block;
            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                entry_block = KrtIrBlockCreate(builder, "entry");
                KrtIrBlockSetCurrent(builder, entry_block);
            } else {
                builder->current_block = entry_block;
            }
            KrtIrGenerateBlock(builder, stmt->data.function_decl.body);
            int has_return = KrtIrCheckHasReturn(stmt->data.function_decl.body);
            if (!has_return) {
                KrtIrReturn(builder, KrtIrImm(builder, 0));
            }
            builder->current_function = saved_function;
            if (builder->type_context) {
                builder->type_context->current_scope = saved_scope;
            }
            if (mangled_name) {
                KRT_FREE(mangled_name);
            }
            break;
        }

        case AST_CLASS_DECLARATION: {
            if (!stmt->data.class_decl.name) {
                break;
            }
            KrtIrPushClassContext(builder, stmt->data.class_decl.name);
            ASTNode* body = stmt->data.class_decl.body;
            if (body) {
                if (body->type == AST_BLOCK) {
                    for (int i = 0; i < body->data.block.statement_count; i++) {
                        ASTNode* member = body->data.block.statements[i];
                        if (!member) continue;
                        if (member->type == AST_ACCESS_MODIFIER) {
                            member = member->data.access_modifier.member;
                            if (!member) continue;
                        }
                        if (member->type == AST_STATIC_FUNCTION_DECLARATION) {
                            KrtIrGenerateStatement(builder, member);
                        } else if (member->type == AST_CONSTRUCTOR_DECLARATION) {
                            KrtIRParam* params = NULL;
                            int pc = member->data.constructor_decl.parameter_count;
                            if (pc >= 0) {
                                params = (KrtIRParam*)KRT_MALLOC((pc + 1) * sizeof(KrtIRParam));
                                if (params) {
                                    params[0].name = "this";
                                    params[0].type = TOKEN_UINT64;
                                    for (int i = 0; i < pc; i++) {
                                        params[i + 1].name = member->data.constructor_decl.parameters[i];
                                        params[i + 1].type = member->data.constructor_decl.parameter_types[i];
                                    }
                                }
                            }
                            char* mangled = KrtIrMangleStaticMember(stmt->data.class_decl.name, "constructor");
                            KrtIRFunction* func = KrtIrFunctionCreate(builder, mangled, params, pc + 1, TOKEN_VOID);
                            KrtIrFunctionSetEntry(builder, func);
                            if (params) {
                                KRT_FREE(params);
                            }
                            KrtIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = KrtIrBlockCreate(builder, "entry");
                                KrtIrBlockSetCurrent(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            KrtIrGenerateBlock(builder, member->data.constructor_decl.body);
                            KrtIrReturn(builder, KrtIrImm(builder, 0));
                            KRT_FREE(mangled);
                        } else if (member->type == AST_DESTRUCTOR_DECLARATION) {
                            KrtIRParam* params = (KrtIRParam*)KRT_MALLOC(sizeof(KrtIRParam));
                            if (params) {
                                params[0].name = "this";
                                params[0].type = TOKEN_UINT64;
                            }
                            char* mangled = KrtIrMangleStaticMember(stmt->data.class_decl.name, "destructor");
                            KrtIRFunction* func = KrtIrFunctionCreate(builder, mangled, params, 1, TOKEN_VOID);
                            KrtIrFunctionSetEntry(builder, func);
                            if (params) {
                                KRT_FREE(params);
                            }
                            KrtIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = KrtIrBlockCreate(builder, "entry");
                                KrtIrBlockSetCurrent(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            KrtIrGenerateBlock(builder, member->data.destructor_decl.body);
                            KrtIrReturn(builder, KrtIrImm(builder, 0));
                            KRT_FREE(mangled);
                        } else if (member->type == AST_FUNCTION_DECLARATION) {
                            KrtIRParam* params = NULL;
                            int pc = member->data.function_decl.parameter_count;
                            if (pc >= 0) {
                                params = (KrtIRParam*)KRT_MALLOC((pc + 1) * sizeof(KrtIRParam));
                                if (params) {
                                    params[0].name = "this";
                                    params[0].type = TOKEN_UINT64;
                                    for (int i = 0; i < pc; i++) {
                                        params[i + 1].name = member->data.function_decl.parameters[i];
                                        params[i + 1].type = member->data.function_decl.parameter_types[i];
                                    }
                                }
                            }
                            const char* base_name = member->data.function_decl.name;
                            char* mangled = KrtIrMangleStaticMember(stmt->data.class_decl.name, base_name);
                            KrtIRFunction* func = KrtIrFunctionCreate(builder, mangled, params, pc + 1, member->data.function_decl.return_type);
                            KrtIrFunctionSetEntry(builder, func);
                            if (params) {
                                KRT_FREE(params);
                            }
                            KrtIRBasicBlock* entry_block = func->entry_block;
                            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                                entry_block = KrtIrBlockCreate(builder, "entry");
                                KrtIrBlockSetCurrent(builder, entry_block);
                            } else {
                                builder->current_block = entry_block;
                            }
                            KrtIrGenerateBlock(builder, member->data.function_decl.body);
                            int has_return = KrtIrCheckHasReturn(member->data.function_decl.body);
                            if (!has_return) {
                                KrtIrReturn(builder, KrtIrImm(builder, 0));
                            }
                            KRT_FREE(mangled);
                        } else if (member->type == AST_STATIC_VARIABLE_DECLARATION) {
                            KrtIrGenerateStatement(builder, member);
                        } else if (member->type == AST_CLASS_DECLARATION) {
                            KrtIrGenerateStatement(builder, member);
                        }
                    }
                } else {
                    ASTNode* member = body;
                    if (member->type == AST_ACCESS_MODIFIER) {
                        member = member->data.access_modifier.member;
                    }
                    if (member && member->type == AST_STATIC_FUNCTION_DECLARATION) {
                        KrtIrGenerateStatement(builder, member);
                    } else if (member && member->type == AST_FUNCTION_DECLARATION) {

                    } else if (member && member->type == AST_STATIC_VARIABLE_DECLARATION) {
                        KrtIrGenerateStatement(builder, member);
                    }
                }
            }
            KrtIrRegisterClassLayout(builder, stmt->data.class_decl.name, stmt->data.class_decl.body);
            KrtIrPopClassContext(builder);
            break;
        }
        case AST_CONSTRUCTOR_DECLARATION:
        case AST_DESTRUCTOR_DECLARATION:
            break;
        case AST_NAMESPACE_DECLARATION: {
            
            const char* namespace_name = stmt->data.namespace_decl.name;
            if (namespace_name) {
                KrtIrPushNamespaceContext(builder, namespace_name);

                ASTNode* body = stmt->data.namespace_decl.body;
                if (body) {
                    if (body->type == AST_BLOCK) {
                        for (int i = 0; i < body->data.block.statement_count; i++) {
                            ASTNode* inner_stmt = body->data.block.statements[i];
                            if (inner_stmt) {
                                KrtIrGenerateStatement(builder, inner_stmt);
                            }
                        }
                    } else {
                        KrtIrGenerateStatement(builder, body);
                    }
                }

                KrtIrPopNamespaceContext(builder);
            }
            break;
        }
        case AST_STATIC_FUNCTION_DECLARATION: {
            KrtIRParam* params = NULL;
            if (stmt->data.static_function_decl.parameter_count > 0) {
                params = (KrtIRParam*)KRT_MALLOC(stmt->data.static_function_decl.parameter_count * sizeof(KrtIRParam));
                if (params) {
                    for (int i = 0; i < stmt->data.static_function_decl.parameter_count; i++) {
                        params[i].name = stmt->data.static_function_decl.parameters[i];
                        params[i].type = stmt->data.static_function_decl.parameter_types[i];
                    }
                }
            }
            const char* current_class = KrtIrCurrentClassContext(builder);
            const char* base_name = stmt->data.static_function_decl.name;
            char* mangled_name = NULL;
            const char* function_name = base_name;
            if (current_class) {
                mangled_name = KrtIrMangleStaticMember(current_class, base_name);
                if (mangled_name) {
                    function_name = mangled_name;
                }
            }
            KrtIRFunction* func = KrtIrFunctionCreate(builder, function_name, params,
                                                       stmt->data.static_function_decl.parameter_count,
                                                       stmt->data.static_function_decl.return_type);
            KrtIrFunctionSetEntry(builder, func);
            if (params) {
                KRT_FREE(params);
            }
            KrtIRBasicBlock* entry_block = func->entry_block;
            if (!entry_block || entry_block->first_inst != NULL || entry_block->next != NULL) {
                entry_block = KrtIrBlockCreate(builder, "entry");
                KrtIrBlockSetCurrent(builder, entry_block);
            } else {
                builder->current_block = entry_block;
            }
            KrtIrGenerateBlock(builder, stmt->data.static_function_decl.body);
            int has_return = KrtIrCheckHasReturn(stmt->data.static_function_decl.body);
            if (!has_return) {
                KrtIrReturn(builder, KrtIrImm(builder, 0));
            }
            if (mangled_name) {
                KRT_FREE(mangled_name);
            }
            break;
        }

        default:
            KrtIrGenerateExpression(builder, stmt);
            return;
    }
}

static int KrtIrCheckHasReturn(ASTNode* node) {
    if (!node) return 0;
    switch (node->type) {
        case AST_RETURN_STATEMENT: return 1;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.statement_count; i++) {
                if (KrtIrCheckHasReturn(node->data.block.statements[i])) return 1;
            }
            return 0;
        case AST_IF_STATEMENT: {
            int then_has = KrtIrCheckHasReturn(node->data.if_stmt.then_branch);
            int else_has = node->data.if_stmt.else_branch && KrtIrCheckHasReturn(node->data.if_stmt.else_branch);
            return then_has && else_has;
        }
        case AST_WHILE_STATEMENT:
        case AST_FOR_STATEMENT:
        case AST_FOREACH_STATEMENT: return 0;
        default: return 0;
    }
}
static void KrtIrGenerateBlock(KrtIRBuilder* builder, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) {
        return;
    }
    for (int i = 0; i < block->data.block.statement_count; i++) {
        KrtIrGenerateStatement(builder, block->data.block.statements[i]);
    }
}
void KrtIrGenerateFromAst(KrtIRBuilder* builder, ASTNode* ast, TypeCheckContext* type_context) {
    if (!builder || !ast) {
        return;
    }
    builder->type_context = type_context;
    
    if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt->type == AST_NAMESPACE_DECLARATION) {
                KrtIrGenerateStatement(builder, stmt);
            }
        }

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt->type == AST_FUNCTION_DECLARATION ||
                stmt->type == AST_STATIC_FUNCTION_DECLARATION ||
                stmt->type == AST_CLASS_DECLARATION) {
                KrtIrGenerateStatement(builder, stmt);
            }
        }

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt->type != AST_FUNCTION_DECLARATION &&
                stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                stmt->type != AST_CLASS_DECLARATION &&
                stmt->type != AST_NAMESPACE_DECLARATION) {
                KrtIrEnsureMainEntry(builder);
                KrtIrGenerateStatement(builder, stmt);
            }
        }
    } else {
        KrtIrEnsureMainEntry(builder);
        KrtIrGenerateStatement(builder, ast);
    }
    if (builder->current_function) {
        int has_main_program_code = 0;
        if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
            for (int i = 0; i < ast->data.block.statement_count; i++) {
                ASTNode* stmt = ast->data.block.statements[i];
                if (stmt->type != AST_FUNCTION_DECLARATION &&
                    stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                    stmt->type != AST_CLASS_DECLARATION &&
                    stmt->type != AST_NAMESPACE_DECLARATION) {
                    has_main_program_code = 1;
                    break;
                }
            }
        } else {
            has_main_program_code = 1;
        }
        if (has_main_program_code) {
            if (ast->type == AST_BLOCK || ast->type == AST_PROGRAM) {
                int main_has_return = 0;
                for (int i = 0; i < ast->data.block.statement_count; i++) {
                    ASTNode* stmt = ast->data.block.statements[i];
                    if (stmt->type != AST_FUNCTION_DECLARATION &&
                        stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                        stmt->type != AST_CLASS_DECLARATION &&
                        stmt->type != AST_NAMESPACE_DECLARATION) {
                        if (KrtIrCheckHasReturn(stmt)) {
                            main_has_return = 1;
                            break;
                        }
                    }
                }
                if (!main_has_return) {
                    KrtIrReturn(builder, KrtIrImm(builder, 0));
                }
            } else if (ast->type != AST_RETURN_STATEMENT) {
                KrtIrReturn(builder, KrtIrImm(builder, 0));
            }
        }
    }
}