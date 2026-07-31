#include "SemanticAnalyzer.h"
#include "TypeChecker.h"
#include "NameMangling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "Core/Utils/OutputCache.h"
#include "Core/Utils/KrtCommon.h"
#include "Accelerator.h"
#include "../Lexer/Tokenizer.h"

#define DEBUG_SYMBOL_TABLE 1

static void semantic_analyzer_register_builtins(SemanticAnalyzer* analyzer) {
    if (!analyzer || !analyzer->symbol_table) return;

    symbol_table_define(analyzer->symbol_table, "timer_start", SYMBOL_FUNCTION, 0, NULL);
    symbol_table_define(analyzer->symbol_table, "timer_start_int", SYMBOL_FUNCTION, 0, NULL);
    symbol_table_define(analyzer->symbol_table, "timer_elapsed", SYMBOL_FUNCTION, 0, NULL);
    symbol_table_define(analyzer->symbol_table, "timer_elapsed_int", SYMBOL_FUNCTION, 0, NULL);
    symbol_table_define(analyzer->symbol_table, "timer_current", SYMBOL_FUNCTION, 0, NULL);
    symbol_table_define(analyzer->symbol_table, "timer_current_int", SYMBOL_FUNCTION, 0, NULL);
}

SemanticAnalyzer* semantic_analyzer_create(void) {
    SemanticAnalyzer* analyzer = KRT_MALLOC(sizeof(SemanticAnalyzer));
    if (!analyzer) {
        return NULL;
    }

    analyzer->symbol_table = symbol_table_create();
    analyzer->global_symbol_table = analyzer->symbol_table;  
    analyzer->ir_builder = NULL;
    analyzer->error_count = 0;
    analyzer->warning_count = 0;
    analyzer->has_entry_point = false;
    analyzer->entry_point_name = NULL;

    semantic_analyzer_register_builtins(analyzer);

    analyzer->class_stack_size = 0;
    analyzer->class_stack_capacity = 8;
    analyzer->class_name_stack = (char**)KRT_CALLOC(analyzer->class_stack_capacity, sizeof(char*));
    if (!analyzer->class_name_stack) {
        KRT_FREE(analyzer);
        return NULL;
    }

    analyzer->generic_registry = generics_create_registry();
    if (!analyzer->generic_registry) {
        KRT_FREE(analyzer->class_name_stack);
        KRT_FREE(analyzer);
        return NULL;
    }
    analyzer->generic_registry_shared = false;
    analyzer->is_unsafe_mode = 0;
    
    analyzer->using_count = 0;
    analyzer->using_capacity = 16;
    analyzer->using_directives = (UsingDirective**)KRT_CALLOC(analyzer->using_capacity, sizeof(UsingDirective*));
    if (!analyzer->using_directives) {
        KRT_FREE(analyzer->class_name_stack);
        KRT_FREE(analyzer);
        return NULL;
    }

    return analyzer;
}

void semantic_analyzer_destroy(SemanticAnalyzer* analyzer) {
    if (!analyzer) {
        return;
    }

    if (analyzer->symbol_table) {
        symbol_table_destroy(analyzer->symbol_table);
        analyzer->symbol_table = NULL;
    }

    if (analyzer->class_name_stack) {
        for (int i = 0; i < analyzer->class_stack_size; i++) {
            if (analyzer->class_name_stack[i]) {
                char* name = analyzer->class_name_stack[i];
                analyzer->class_name_stack[i] = NULL;
                KRT_FREE(name);
            }
        }
        char** stack = analyzer->class_name_stack;
        analyzer->class_name_stack = NULL;
        KRT_FREE(stack);
    }

    if (analyzer->generic_registry && !analyzer->generic_registry_shared) {
        generics_destroy_registry(analyzer->generic_registry);
        analyzer->generic_registry = NULL;
    }
    
    if (analyzer->using_directives) {
        for (int i = 0; i < analyzer->using_count; i++) {
            UsingDirective* directive = analyzer->using_directives[i];
            if (directive) {
                if (directive->alias) KRT_FREE(directive->alias);
                if (directive->namespace_path) {
                    for (int j = 0; j < directive->path_length; j++) {
                        KRT_FREE(directive->namespace_path[j]);
                    }
                    KRT_FREE(directive->namespace_path);
                }
                KRT_FREE(directive);
            }
        }
        KRT_FREE(analyzer->using_directives);
    }

    KRT_FREE(analyzer->entry_point_name);
    analyzer->entry_point_name = NULL;
    KRT_FREE(analyzer);
}

void semantic_analyzer_push_class_context(SemanticAnalyzer* analyzer, const char* class_name) {
    if (!analyzer || !class_name) {
        return;
    }

    if (analyzer->class_stack_size >= analyzer->class_stack_capacity) {
        int new_capacity = analyzer->class_stack_capacity * 2;
        char** new_stack = (char**)KRT_REALLOC(analyzer->class_name_stack, sizeof(char*) * new_capacity);
        if (!new_stack) {
            return;
        }
        analyzer->class_name_stack = new_stack;
        analyzer->class_stack_capacity = new_capacity;
    }

    analyzer->class_name_stack[analyzer->class_stack_size] = KRT_STRDUP(class_name);
    analyzer->class_stack_size++;
}

void semantic_analyzer_pop_class_context(SemanticAnalyzer* analyzer) {
    if (!analyzer || analyzer->class_stack_size <= 0) {
        return;
    }

    analyzer->class_stack_size--;
    KRT_FREE(analyzer->class_name_stack[analyzer->class_stack_size]);
    analyzer->class_name_stack[analyzer->class_stack_size] = NULL;
}

static void semantic_analyzer_add_using_directive(SemanticAnalyzer* analyzer, ASTNode* using_node) {
    if (!analyzer || !using_node) return;
    
    if (analyzer->using_count >= analyzer->using_capacity) {
        analyzer->using_capacity *= 2;
        analyzer->using_directives = (UsingDirective**)KRT_REALLOC(
            analyzer->using_directives, 
            analyzer->using_capacity * sizeof(UsingDirective*)
        );
    }
    
    UsingDirective* directive = (UsingDirective*)KRT_MALLOC(sizeof(UsingDirective));
    if (!directive) return;
    
    directive->alias = NULL;
    directive->namespace_path = NULL;
    directive->path_length = 0;
    directive->is_alias = 0;
    
    if (using_node->data.using_directive.alias) {
        directive->alias = KRT_STRDUP(using_node->data.using_directive.alias);
    }
    
    directive->is_alias = using_node->data.using_directive.is_alias;
    directive->path_length = using_node->data.using_directive.path_length;
    
    if (using_node->data.using_directive.namespace_path) {
        directive->namespace_path = (char**)KRT_MALLOC(
            directive->path_length * sizeof(char*)
        );
        for (int i = 0; i < directive->path_length; i++) {
            directive->namespace_path[i] = KRT_STRDUP(
                using_node->data.using_directive.namespace_path[i]
            );
        }
    }
    
    analyzer->using_directives[analyzer->using_count++] = directive;
}

static SymbolEntry* semantic_analyzer_lookup_namespace(SemanticAnalyzer* analyzer,
                                                       char** namespace_path,
                                                       int path_length) {
    if (!analyzer || !namespace_path || path_length <= 0) return NULL;

    SymbolEntry* current = NULL;
    
    SymbolTable* current_table = analyzer->global_symbol_table ? analyzer->global_symbol_table : analyzer->symbol_table;

    for (int i = 0; i < path_length; i++) {
        current = symbol_table_lookup(current_table, namespace_path[i]);

        if (!current) {
            return NULL;
        }

        if (i < path_length - 1) {
            if (current->type != SYMBOL_NAMESPACE) {
                return NULL;
            }
            if (!current->nested_table) {
                return NULL;
            }
            
            current_table = current->nested_table;
        }
    }

    if (current && current->type != SYMBOL_NAMESPACE) {
        return NULL;
    }

    return current;
}



SymbolEntry* semantic_analyzer_lookup_qualified_name(SemanticAnalyzer* analyzer, 
                                                     char** parts, 
                                                     int part_count) {
    if (!analyzer || !parts || part_count <= 0) return NULL;
    
    SymbolEntry* result = semantic_analyzer_lookup_namespace(analyzer, parts, part_count);
    if (result) {
        return result;
    }
    
    if (part_count >= 2) {
        SymbolEntry* ns = semantic_analyzer_lookup_namespace(analyzer, parts, part_count - 1);
        if (ns && ns->nested_table) {
            result = symbol_table_lookup(ns->nested_table, parts[part_count - 1]);
            if (result) {
                return result;
            }
        }
    }
    
    return NULL;
}

static SymbolEntry* semantic_analyzer_lookup_with_usings(SemanticAnalyzer* analyzer, const char* name) {
    if (!analyzer || !name) return NULL;

    SymbolEntry* result = symbol_table_lookup(analyzer->symbol_table, name);
    if (result) return result;

    for (int i = 0; i < analyzer->using_count; i++) {
        UsingDirective* directive = analyzer->using_directives[i];
        if (!directive || directive->path_length <= 0) continue;

        SymbolEntry* ns = semantic_analyzer_lookup_namespace(analyzer, directive->namespace_path, directive->path_length);
        if (ns && ns->nested_table) {
            result = symbol_table_lookup(ns->nested_table, name);
            if (result) return result;
        }
    }

    return NULL;
}

const char* semantic_analyzer_get_current_class_context(SemanticAnalyzer* analyzer) {
    if (!analyzer || analyzer->class_stack_size <= 0) {
        return NULL;
    }
    return analyzer->class_name_stack[analyzer->class_stack_size - 1];
}

static SymbolEntry* semantic_analyzer_lookup_class(SemanticAnalyzer* analyzer, const char* class_name) {
    if (!analyzer || !class_name) return NULL;

    SymbolTable* table = analyzer->symbol_table;
    while (table) {
        SymbolEntry* result = symbol_table_lookup(table, class_name);
        if (result && result->type == SYMBOL_CLASS) {
            return result;
        }
        table = table->parent_table;
    }

    return NULL;
}

static KrtTokenType semantic_analyzer_infer_expression_type(SemanticAnalyzer* analyzer, ASTNode* expr) {
    if (!expr) return TOKEN_INT32;

    switch (expr->type) {
        case AST_NUMBER: {
            double v = expr->data.number_value;
            int64_t iv = (int64_t)v;
            if (v != (double)iv) return TOKEN_FLOAT64;
            return TOKEN_INT32;
        }
        case AST_STRING:
            return TOKEN_STRING;
        case AST_BOOLEAN:
            return TOKEN_BOOL;
        case AST_IDENTIFIER: {
            SymbolEntry* sym = symbol_table_lookup_scope_chain(analyzer->symbol_table, expr->data.identifier_name);
            if (sym && sym->value_type != TOKEN_EOF) {
                return sym->value_type;
            }
            return TOKEN_INT32;
        }
        case AST_CAST_EXPRESSION:
            return expr->data.cast_expr.target_type;
        case AST_UNARY_OPERATION:
            return semantic_analyzer_infer_expression_type(analyzer, expr->data.unary_op.operand);
        case AST_BINARY_OPERATION: {
            KrtTokenType left = semantic_analyzer_infer_expression_type(analyzer, expr->data.binary_op.left);
            KrtTokenType right = semantic_analyzer_infer_expression_type(analyzer, expr->data.binary_op.right);
            if (left == TOKEN_FLOAT64 || right == TOKEN_FLOAT64) return TOKEN_FLOAT64;
            if (left == TOKEN_FLOAT32 || right == TOKEN_FLOAT32) return TOKEN_FLOAT32;
            if (left == TOKEN_STRING || right == TOKEN_STRING) return TOKEN_STRING;
            return TOKEN_INT32;
        }
        default:
            return TOKEN_INT32;
    }
}

void semantic_analyzer_add_error(SemanticAnalyzer* analyzer, const char* format, ...) {
    if (!analyzer) {
        return;
    }

    analyzer->error_count++;

    va_list args;
    va_start(args, format);
    KrtOutputCacheAddError("Error: ");
    KrtOutputCacheAddErrorv(format, args);
    KrtOutputCacheAddError("\n");
    va_end(args);
}

void semantic_analyzer_add_warning(SemanticAnalyzer* analyzer, const char* format, ...) {
    if (!analyzer) {
        return;
    }

    analyzer->warning_count++;

    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    KrtOutputCacheAdd("[SEMANTIC WARNING] ");
    KrtOutputCacheAdd(buffer);
    KrtOutputCacheAdd("\n");
    
    va_end(args);
}

bool semantic_analyzer_analyze_expression(SemanticAnalyzer* analyzer, ASTNode* expr) {
    if (!analyzer || !expr) {
        return false;
    }

    switch (expr->type) {
        case AST_NUMBER:
        case AST_BOOLEAN:
            return true;

        case AST_IDENTIFIER: {
            const char* var_name = expr->data.identifier_name;
            
            if (!analyzer->is_unsafe_mode && var_name[0] == '_' && strncmp(var_name, "__lambda_", 9) != 0) {
                semantic_analyzer_add_error(analyzer, "Direct access to internal symbol '%s' is restricted. Use unsafe(\"%s\", ...) instead.", var_name, var_name);
                return false;
            }
            
            SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined identifier: %s", var_name);
                return false;
            }
            
            return true;
        }

        case AST_UNSAFE_CALL: {
            int old_mode = analyzer->is_unsafe_mode;
            analyzer->is_unsafe_mode = 1;
            bool result = semantic_analyzer_analyze_expression(analyzer, expr->data.unsafe_call.expression);
            analyzer->is_unsafe_mode = old_mode;
            return result;
        }

        case AST_BINARY_OPERATION: {
            bool left_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.binary_op.left);
            bool right_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.binary_op.right);
            return left_ok && right_ok;
        }

        case AST_UNARY_OPERATION:
            return semantic_analyzer_analyze_expression(analyzer, expr->data.unary_op.operand);

        case AST_CALL:
            return semantic_analyzer_analyze_function_call(analyzer, expr);

        case AST_ASSIGNMENT: {
            const char* var_name = expr->data.assignment.name;
            SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return semantic_analyzer_analyze_expression(analyzer, expr->data.assignment.value);
        }

        case AST_COMPOUND_ASSIGNMENT: {
            const char* var_name = expr->data.compound_assignment.name;
            SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return semantic_analyzer_analyze_expression(analyzer, expr->data.compound_assignment.value);
        }

        case AST_TERNARY_OPERATION: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.condition);
            bool true_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.true_value);
            bool false_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.ternary_op.false_value);
            return cond_ok && true_ok && false_ok;
        }

        case AST_NEW_EXPRESSION: {
            
            if (!expr || !expr->data.new_expr.class_name) {
                semantic_analyzer_add_error(analyzer, "Invalid new expression node");
                return false;
            }
            
            KrtTokenType type_token = expr->data.new_expr.type_token;
            const char* class_name = expr->data.new_expr.class_name;
            
            if (type_token == TOKEN_INT32) {
                
                if (expr->data.new_expr.argument_count > 0 && !expr->data.new_expr.arguments) {
                    return true; 
                }
                
                for (int i = 0; i < expr->data.new_expr.argument_count; i++) {
                    if (expr->data.new_expr.arguments[i] && 
                        !semantic_analyzer_analyze_expression(analyzer, expr->data.new_expr.arguments[i])) {
                        return false;
                    }
                }
                return true;
            }

            if (!class_name || strlen(class_name) == 0) {
                semantic_analyzer_add_error(analyzer, "Invalid class name for new expression");
                return false;
            }
            
            SymbolEntry* class_symbol = semantic_analyzer_lookup_with_usings(analyzer, class_name);
            if (!class_symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined class: %s", class_name);
                return false;
            }

            if (class_symbol->type != SYMBOL_CLASS) {
                semantic_analyzer_add_error(analyzer, "%s is not a class", class_name);
                return false;
            }

            for (int i = 0; i < expr->data.new_expr.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, expr->data.new_expr.arguments[i])) {
                    return false;
                }
            }

            return true;
        }

        case AST_NEW_ARRAY_EXPRESSION: {
            
            KrtTokenType type_token = expr->data.new_array_expr.type_token;
            
            if (type_token >= TOKEN_INT8 && type_token <= TOKEN_VOID) {
                
                if (!semantic_analyzer_analyze_expression(analyzer, expr->data.new_array_expr.size)) {
                    return false;
                }
                return true;
            }
            
            semantic_analyzer_add_error(analyzer, "Array creation only supported for built-in types");
            return false;
        }

        case AST_STATIC_METHOD_CALL: {
            const char* class_name = expr->data.static_call.class_name;
            const char* method_name = expr->data.static_call.method_name;

            SymbolEntry* class_symbol = semantic_analyzer_lookup_class(analyzer, class_name);
            if (!class_symbol || class_symbol->type != SYMBOL_CLASS) {
                semantic_analyzer_add_error(analyzer, "Undefined class: %s", class_name);
                return false;
            }

            for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, expr->data.static_call.arguments[i])) {
                    return false;
                }
            }

            SymbolEntry* method_symbol = NULL;
            char* mangled_name = NULL;
            if (class_symbol->nested_table) {
                KrtTokenType* arg_types = NULL;
                if (expr->data.static_call.argument_count > 0) {
                    arg_types = (KrtTokenType*)KRT_MALLOC(sizeof(KrtTokenType) * expr->data.static_call.argument_count);
                    if (!arg_types) {
                        semantic_analyzer_add_error(analyzer, "Memory allocation failed");
                        return false;
                    }
                    for (int i = 0; i < expr->data.static_call.argument_count; i++) {
                        arg_types[i] = semantic_analyzer_infer_expression_type(analyzer, expr->data.static_call.arguments[i]);
                    }
                }
                const char* ns_path[2] = { class_name, NULL };
                mangled_name = name_mangle_function(ns_path, method_name, arg_types,
                                                       expr->data.static_call.argument_count);
                if (arg_types) KRT_FREE(arg_types);

                if (mangled_name) {
                    method_symbol = symbol_table_lookup(class_symbol->nested_table, mangled_name);
                }
            }

            if (!method_symbol) {
                method_symbol = symbol_table_lookup_in_namespace(analyzer->symbol_table, class_name, method_name);
                if (!method_symbol) {
                    semantic_analyzer_add_error(analyzer, "Undefined static method: %s::%s", class_name, method_name);
                    if (mangled_name) KRT_FREE(mangled_name);
                    return false;
                }
            }

            if (method_symbol->type != SYMBOL_FUNCTION) {
                semantic_analyzer_add_error(analyzer, "%s::%s is not a static method", class_name, method_name);
                if (mangled_name) KRT_FREE(mangled_name);
                return false;
            }

            if (mangled_name) {
                if (expr->data.static_call.resolved_mangled_name) {
                    KRT_FREE(expr->data.static_call.resolved_mangled_name);
                }
                expr->data.static_call.resolved_mangled_name = mangled_name;
            }

            return true;
        }

        case AST_MEMBER_ACCESS: {
            bool object_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.member_access.object);
            if (!object_ok) {
                return false;
            }

            ASTNode* object = expr->data.member_access.object;
            const char* member_name = expr->data.member_access.member_name;

            if (object && (object->type == AST_THIS ||
                           (object->type == AST_IDENTIFIER && strcmp(object->data.identifier_name, "this") == 0))) {
                const char* current_class = semantic_analyzer_get_current_class_context(analyzer);
                if (current_class) {
                    SymbolEntry* member = symbol_table_lookup_scope_chain(analyzer->symbol_table, member_name);
                    if (member && (member->type == SYMBOL_FIELD || member->type == SYMBOL_FUNCTION ||
                                   member->type == SYMBOL_STATIC_FIELD || member->type == SYMBOL_VARIABLE)) {
                        if (expr->data.member_access.resolved_class_name) {
                            KRT_FREE(expr->data.member_access.resolved_class_name);
                        }
                        expr->data.member_access.resolved_class_name = KRT_STRDUP(current_class);
                        return true;
                    }
                    semantic_analyzer_add_error(analyzer, "Undefined member '%s' in class '%s'", member_name, current_class);
                    return false;
                }
            }

            if (object && object->type == AST_IDENTIFIER) {
                const char* class_name = object->data.identifier_name;
                SymbolEntry* class_symbol = semantic_analyzer_lookup_class(analyzer, class_name);
                if (class_symbol && class_symbol->type == SYMBOL_CLASS) {
                    char* mangled_name = name_mangle_simple(class_name, member_name);
                    SymbolEntry* member = NULL;
                    if (class_symbol->nested_table && mangled_name) {
                        member = symbol_table_lookup(class_symbol->nested_table, mangled_name);
                    }
                    if (!member && class_symbol->nested_table) {
                        member = symbol_table_lookup(class_symbol->nested_table, member_name);
                    }
                    if (member && (member->type == SYMBOL_STATIC_FIELD || member->type == SYMBOL_FUNCTION)) {
                        if (expr->data.member_access.resolved_class_name) {
                            KRT_FREE(expr->data.member_access.resolved_class_name);
                        }
                        if (expr->data.member_access.resolved_mangled_name) {
                            KRT_FREE(expr->data.member_access.resolved_mangled_name);
                        }
                        expr->data.member_access.resolved_class_name = KRT_STRDUP(class_name);
                        expr->data.member_access.resolved_mangled_name = mangled_name;
                        return true;
                    }
                    if (mangled_name) KRT_FREE(mangled_name);
                }
            }

            return object_ok;
        }

        case AST_ARRAY_ACCESS: {
            bool array_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.array_access.array);
            bool index_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.array_access.index);
            
            if (array_ok && expr->data.array_access.array->type == AST_IDENTIFIER) {
                const char* array_name = expr->data.array_access.array->data.identifier_name;
                SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, array_name);
                if (symbol && !symbol->is_array) {
                    semantic_analyzer_add_error(analyzer, "Attempting to access non-array as array");
                    return false;
                }
            }
            
            return array_ok && index_ok;
        }

        case AST_LAMBDA_EXPRESSION: {
            
            symbol_table_push_scope(analyzer->symbol_table);

            for (int i = 0; i < expr->data.lambda_expr.parameter_count; i++) {
                const char* param_name = expr->data.lambda_expr.parameters[i];
                SymbolEntry* param = symbol_table_define(analyzer->symbol_table, param_name, SYMBOL_VARIABLE, 0, NULL);
                if (!param) {
                    semantic_analyzer_add_error(analyzer, "Failed to define lambda parameter: %s", param_name);
                    symbol_table_pop_scope(analyzer->symbol_table);
                    return false;
                }
            }

            bool body_ok = false;
            if (expr->data.lambda_expr.expression) {
                body_ok = semantic_analyzer_analyze_expression(analyzer, expr->data.lambda_expr.expression);
            } else if (expr->data.lambda_expr.body) {
                body_ok = semantic_analyzer_analyze_statement(analyzer, expr->data.lambda_expr.body);
            }

            symbol_table_pop_scope(analyzer->symbol_table);

            return body_ok;
        }

        case AST_LINQ_QUERY: {
            bool ok = true;
            
            if (expr->data.linq_query.from_clause) {
                ok = ok && semantic_analyzer_analyze_expression(analyzer, 
                    expr->data.linq_query.from_clause->data.linq_from.source);
            }
            
            for (int i = 0; i < expr->data.linq_query.clause_count; i++) {
                ASTNode* clause = expr->data.linq_query.clauses[i];
                if (clause->type == AST_LINQ_WHERE) {
                    ok = ok && semantic_analyzer_analyze_expression(analyzer, 
                        clause->data.linq_where.condition);
                } else if (clause->type == AST_LINQ_ORDERBY) {
                    ok = ok && semantic_analyzer_analyze_expression(analyzer, 
                        clause->data.linq_orderby.expression);
                }
            }
            
            if (expr->data.linq_query.select_clause) {
                ok = ok && semantic_analyzer_analyze_expression(analyzer, 
                    expr->data.linq_query.select_clause->data.linq_select.expression);
            }
            
            return ok;
        }

        default:
            return true;
    }
}

bool semantic_analyzer_analyze_function_call(SemanticAnalyzer* analyzer, ASTNode* call_expr) {
    if (!analyzer || !call_expr || call_expr->type != AST_CALL) {
        return false;
    }

    char* func_name = NULL;
    
    if (call_expr->data.call.object && call_expr->data.call.object->type == AST_IDENTIFIER) {
        const char* class_name = call_expr->data.call.object->data.identifier_name;
        const char* method_name = call_expr->data.call.name;
        
        if (strstr(method_name, "__") != NULL) {
            
            func_name = KRT_STRDUP(method_name);
        } else {
            
            size_t total_len = strlen(class_name) + strlen(method_name) + 3; 
            func_name = KRT_MALLOC(total_len);
            if (func_name) {
                snprintf(func_name, total_len, "%s__%s", class_name, method_name);
                
                call_expr->data.call.resolved_class_name = KRT_STRDUP(class_name);
            }
        }
    } else {
        
        func_name = KRT_STRDUP(call_expr->data.call.name);
    }
    
    if (!func_name) {
        semantic_analyzer_add_error(analyzer, "Memory allocation failed for function name");
        return false;
    }

    SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, func_name);

    if (!symbol) {
        
        SymbolEntry* var_symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, func_name);
        if (var_symbol && var_symbol->type == SYMBOL_VARIABLE) {
            
            KRT_FREE(func_name);
            
            for (int i = 0; i < call_expr->data.call.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, call_expr->data.call.arguments[i])) {
                    return false;
                }
            }
            return true;
        }

        symbol = symbol_table_declare(analyzer->symbol_table, func_name, SYMBOL_FUNCTION, 0);
        if (symbol) {
            symbol->state = SYMBOL_FORWARD_REF;
        }
        KRT_FREE(func_name);
        return true;
    }

    if (symbol->type != SYMBOL_FUNCTION) {
        
        if (symbol->type == SYMBOL_VARIABLE) {
            
            KRT_FREE(func_name);
            
            for (int i = 0; i < call_expr->data.call.argument_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, call_expr->data.call.arguments[i])) {
                    return false;
                }
            }
            return true;
        }
        
        semantic_analyzer_add_error(analyzer, "%s is not a function", func_name);
        KRT_FREE(func_name);
        return false;
    }
    
    if (!call_expr->data.call.object) {
        call_expr->data.call.name = func_name;  
    } else {
        KRT_FREE(func_name);
    }

    for (int i = 0; i < call_expr->data.call.argument_count; i++) {
        if (!semantic_analyzer_analyze_expression(analyzer, call_expr->data.call.arguments[i])) {
            return false;
        }
    }

    return true;
}

bool semantic_analyzer_analyze_variable_decl(SemanticAnalyzer* analyzer, ASTNode* var_decl) {
    if (!analyzer || !var_decl) {
        return false;
    }

    const char* var_name = var_decl->data.variable_decl.name;

    printf("[DEBUG SEM] Analyzing variable declaration: %s (is_let=%d, type=%d)\n", 
           var_name ? var_name : "NULL",
           var_decl->data.variable_decl.is_let,
           var_decl->data.variable_decl.type);
           
    int is_let = var_decl->data.variable_decl.is_let;

    if (is_let) {
        
        if (analyzer->symbol_table->current_scope->scope_level == 1) {
            semantic_analyzer_add_error(analyzer, "let cannot be used at global scope, use var instead");
            return false;
        }
    }

    SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, var_name);
    if (existing) {
        semantic_analyzer_add_error(analyzer, "Variable %s already declared", var_name);
        return false;
    }

    if (var_decl->data.variable_decl.template_instantiation_type) {
        
        char* template_type = var_decl->data.variable_decl.template_instantiation_type;
        
        char* bracket_pos = strchr(template_type, '<');
        if (bracket_pos) {
            
            int base_type_len = bracket_pos - template_type;
            char* base_type = KRT_MALLOC(base_type_len + 1);
            if (base_type) {
                strncpy(base_type, template_type, base_type_len);
                base_type[base_type_len] = '\0';
                
                char* args_start = bracket_pos + 1;
                char* args_end = strrchr(args_start, '>');
                if (args_end) {
                    int args_len = args_end - args_start;
                    char* args_str = KRT_MALLOC(args_len + 1);
                    if (args_str) {
                        strncpy(args_str, args_start, args_len);
                        args_str[args_len] = '\0';
                        
                        char** type_args = KRT_MALLOC(sizeof(char*));
                        int arg_count = 0;
                        
                        if (args_str && strlen(args_str) > 0) {
                            type_args[0] = KRT_STRDUP(args_str);
                            arg_count = 1;
                        }
                        
                        bool valid = semantic_analyzer_analyze_generic_instantiation(analyzer, base_type, 
                                                                                    (const char**)type_args, arg_count);
                        
                        for (int i = 0; i < arg_count; i++) {
                            if (type_args[i]) {
                                KRT_FREE(type_args[i]);
                            }
                        }
                        KRT_FREE(type_args);
                        KRT_FREE(args_str);
                        KRT_FREE(base_type);
                        
                        if (!valid) {
                            semantic_analyzer_add_error(analyzer, "Invalid template instantiation: %s", template_type);
                            return false;
                        }
                    } else {
                        KRT_FREE(base_type);
                    }
                } else {
                    KRT_FREE(base_type);
                }
            }
        }
    }

    const char* current_class = semantic_analyzer_get_current_class_context(analyzer);
    SymbolType symbol_type = SYMBOL_VARIABLE;
    if (current_class) {
        symbol_type = SYMBOL_FIELD;
    }
    
    SymbolEntry* symbol = symbol_table_define(analyzer->symbol_table, var_name,
                                             symbol_type, 0, NULL);
    if (!symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to declare variable %s", var_name);
        return false;
    }
    symbol->value_type = var_decl->data.variable_decl.type;
    
    if (var_decl->data.variable_decl.is_array) {
        symbol->is_array = 1;

        if (var_decl->data.variable_decl.array_size) {
            if (!semantic_analyzer_analyze_expression(analyzer, var_decl->data.variable_decl.array_size)) {
                return false;
            }
        }
    } else if (var_decl->data.variable_decl.value &&
               var_decl->data.variable_decl.value->type == AST_ARRAY_LITERAL) {
        symbol->is_array = 1;
    }

    if (var_decl->data.variable_decl.value) {
        return semantic_analyzer_analyze_expression(analyzer, var_decl->data.variable_decl.value);
    }

    return true;
}

bool semantic_analyzer_analyze_static_variable_decl(SemanticAnalyzer* analyzer, ASTNode* var_decl) {
    if (!analyzer || !var_decl) {
        return false;
    }

    const char* var_name = var_decl->data.static_variable_decl.name;
    const char* current_class = semantic_analyzer_get_current_class_context(analyzer);

    const char* symbol_name = var_name;
    char* mangled_name = NULL;
    if (current_class) {
        mangled_name = name_mangle_simple(current_class, var_name);
        if (mangled_name) {
            symbol_name = mangled_name;
        }
    }

    SymbolEntry* symbol = symbol_table_define(analyzer->symbol_table, symbol_name,
                                              SYMBOL_STATIC_FIELD, 0, NULL);

    if (mangled_name) {
        KRT_FREE(mangled_name);
    }

    if (!symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to declare static variable %s", var_name);
        return false;
    }
    symbol->value_type = var_decl->data.static_variable_decl.type;

    if (var_decl->data.static_variable_decl.value) {
        return semantic_analyzer_analyze_expression(analyzer, var_decl->data.static_variable_decl.value);
    }

    return true;
}

bool semantic_analyzer_analyze_statement(SemanticAnalyzer* analyzer, ASTNode* stmt) {
    if (!analyzer || !stmt) {
        return false;
    }
    
    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION:
            return semantic_analyzer_analyze_variable_decl(analyzer, stmt);

        case AST_STATIC_VARIABLE_DECLARATION:
            return semantic_analyzer_analyze_static_variable_decl(analyzer, stmt);

        case AST_ASSIGNMENT: {
            const char* var_name = stmt->data.assignment.name;
            SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return semantic_analyzer_analyze_expression(analyzer, stmt->data.assignment.value);
        }

        case AST_COMPOUND_ASSIGNMENT: {
            const char* var_name = stmt->data.compound_assignment.name;
            SymbolEntry* symbol = symbol_table_lookup_scope_chain(analyzer->symbol_table, var_name);
            if (!symbol) {
                semantic_analyzer_add_error(analyzer, "Undefined variable: %s", var_name);
                return false;
            }
            return semantic_analyzer_analyze_expression(analyzer, stmt->data.compound_assignment.value);
        }

        case AST_ARRAY_ASSIGNMENT: {
            bool array_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_assignment.array);
            bool index_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_assignment.index);
            bool value_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_assignment.value);
            
            if (array_ok && stmt->data.array_assignment.array->type == AST_IDENTIFIER) {
                const char* array_name = stmt->data.array_assignment.array->data.identifier_name;
                SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, array_name);
                if (symbol && !symbol->is_array) {
                    semantic_analyzer_add_error(analyzer, "Attempting to assign to non-array as array");
                    return false;
                }
            }
            
            return array_ok && index_ok && value_ok;
        }

        case AST_ARRAY_COMPOUND_ASSIGNMENT: {
            bool array_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_compound_assignment.array);
            bool index_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_compound_assignment.index);
            bool value_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.array_compound_assignment.value);
            
            if (array_ok && stmt->data.array_compound_assignment.array->type == AST_IDENTIFIER) {
                const char* array_name = stmt->data.array_compound_assignment.array->data.identifier_name;
                SymbolEntry* symbol = symbol_table_lookup(analyzer->symbol_table, array_name);
                if (symbol && !symbol->is_array) {
                    semantic_analyzer_add_error(analyzer, "Attempting to compound assign to non-array as array");
                    return false;
                }
            }
            
            return array_ok && index_ok && value_ok;
        }

        case AST_RETURN_STATEMENT:
            if (stmt->data.return_stmt.value) {
                return semantic_analyzer_analyze_expression(analyzer, stmt->data.return_stmt.value);
            }
            return true;

        case AST_IF_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.if_stmt.condition);
            symbol_table_push_scope(analyzer->symbol_table);
            bool then_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.if_stmt.then_branch);
            symbol_table_pop_scope(analyzer->symbol_table);
            bool else_ok = true;
            if (stmt->data.if_stmt.else_branch) {
                symbol_table_push_scope(analyzer->symbol_table);
                else_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.if_stmt.else_branch);
                symbol_table_pop_scope(analyzer->symbol_table);
            }
            return cond_ok && then_ok && else_ok;
        }

        case AST_WHILE_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.while_stmt.condition);
            symbol_table_push_scope(analyzer->symbol_table);
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.while_stmt.body);
            symbol_table_pop_scope(analyzer->symbol_table);
            return cond_ok && body_ok;
        }

        case AST_FOR_STATEMENT: {
            symbol_table_push_scope(analyzer->symbol_table);
            bool init_ok = true;
            if (stmt->data.for_stmt.init) {
                init_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.for_stmt.init);
            }
            bool cond_ok = true;
            if (stmt->data.for_stmt.condition) {
                cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.for_stmt.condition);
            }
            bool increment_ok = true;
            if (stmt->data.for_stmt.increment) {
                if (stmt->data.for_stmt.increment->type == AST_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_COMPOUND_ASSIGNMENT ||
                    stmt->data.for_stmt.increment->type == AST_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_STATIC_VARIABLE_DECLARATION ||
                    stmt->data.for_stmt.increment->type == AST_BLOCK) {
                    increment_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.for_stmt.increment);
                } else {
                    increment_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.for_stmt.increment);
                }
            }
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.for_stmt.body);
            symbol_table_pop_scope(analyzer->symbol_table);
            return init_ok && cond_ok && increment_ok && body_ok;
        }

        case AST_FOREACH_STATEMENT: {
            symbol_table_push_scope(analyzer->symbol_table);
            
            SymbolEntry* loop_var = symbol_table_define(analyzer->symbol_table, stmt->data.foreach_stmt.var_name, SYMBOL_VARIABLE, 0, NULL);
            if (!loop_var) {
                semantic_analyzer_add_error(analyzer, "Failed to define loop variable: %s", stmt->data.foreach_stmt.var_name);
                symbol_table_pop_scope(analyzer->symbol_table);
                return false;
            }
            bool iterable_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.foreach_stmt.iterable);
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.foreach_stmt.body);
            symbol_table_pop_scope(analyzer->symbol_table);
            return iterable_ok && body_ok;
        }

        case AST_USING_DIRECTIVE: {
            
            if (stmt->data.using_directive.path_length == 0) {
                semantic_analyzer_add_error(analyzer, "Using directive must have at least one namespace part");
                return false;
            }
            
            if (stmt->data.using_directive.is_alias) {
                if (!stmt->data.using_directive.alias) {
                    semantic_analyzer_add_error(analyzer, "Using alias must have an alias name");
                    return false;
                }
            }
            
            semantic_analyzer_add_using_directive(analyzer, stmt);
            
            return true;
        }

        case AST_NAMESPACE_DECLARATION: {
            const char* namespace_name = stmt->data.namespace_decl.name;

            printf("[DEBUG SEM] Processing namespace: %s\n", namespace_name ? namespace_name : "NULL");
            
            SymbolEntry* ns_entry = symbol_table_lookup_current_scope(analyzer->symbol_table, namespace_name);
            bool is_new_namespace = false;
            SymbolTable* prev_table = analyzer->symbol_table;
            if (!ns_entry) {
                ns_entry = symbol_table_define(analyzer->symbol_table, namespace_name, SYMBOL_NAMESPACE, 0, NULL);
                if (ns_entry && !ns_entry->nested_table) {
                    ns_entry->nested_table = symbol_table_create();
                    if (ns_entry->nested_table) {
                        ns_entry->nested_table->parent_table = prev_table;
                    }
                }
                is_new_namespace = true;
            } else if (ns_entry->type != SYMBOL_NAMESPACE) {
                semantic_analyzer_add_error(analyzer, "'%s' is already declared as a different symbol type", namespace_name);
                return false;
            }
            
            SymbolScope* prev_scope = analyzer->symbol_table->current_scope;
            if (ns_entry && ns_entry->nested_table) {
                
                analyzer->symbol_table = ns_entry->nested_table;
                analyzer->symbol_table->current_scope = ns_entry->nested_table->global_scope;
            }
            
            bool body_ok = true;
            ASTNode* body_node = stmt->data.namespace_decl.body;
            
            printf("[DEBUG SEM] Namespace %s has body: %s, type=%d\n", 
                   namespace_name,
                   body_node ? "YES" : "NO",
                   body_node ? body_node->type : -1);
                   
            if (body_node && body_node->type == AST_BLOCK) {
                printf("[DEBUG SEM] Namespace %s has %d statements\n", namespace_name, body_node->data.block.statement_count);
                for (int i = 0; i < body_node->data.block.statement_count && i < 10; i++) {
                    ASTNode* ns_stmt = body_node->data.block.statements[i];
                    printf("[DEBUG SEM] Namespace %s statement[%d]: type=%d\n", namespace_name, i, ns_stmt ? ns_stmt->type : -1);
                }
                
                if (body_node->type == AST_BLOCK) {
                    for (int i = 0; i < body_node->data.block.statement_count; i++) {
                        ASTNode* decl = body_node->data.block.statements[i];
                        if (decl && decl->type == AST_ACCESS_MODIFIER) {
                            decl = decl->data.access_modifier.member;
                        }
                        if (decl && decl->type == AST_CLASS_DECLARATION) {
                            const char* class_name = decl->data.class_decl.name;
                            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, class_name);
                            if (!existing) {
                                SymbolEntry* class_sym = symbol_table_declare(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0);
                                if (class_sym && !class_sym->nested_table) {
                                    class_sym->nested_table = symbol_table_create();
                                }
                            }
                        }
                    }
                    if (is_new_namespace) {
                        for (int i = 0; i < body_node->data.block.statement_count; i++) {
                            ASTNode* decl = body_node->data.block.statements[i];
                            if (!decl) continue;
                            bool stmt_ok = semantic_analyzer_analyze_statement(analyzer, decl);
                            if (!stmt_ok) {
                                body_ok = false;
                            }
                        }
                    }
                } else if (body_node->type == AST_ACCESS_MODIFIER) {
                    ASTNode* inner_decl = body_node->data.access_modifier.member;
                    if (inner_decl && inner_decl->type == AST_CLASS_DECLARATION) {
                        const char* class_name = inner_decl->data.class_decl.name;
                        SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, class_name);
                        if (!existing) {
                            SymbolEntry* class_sym = symbol_table_declare(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0);
                            if (class_sym && !class_sym->nested_table) {
                                class_sym->nested_table = symbol_table_create();
                            }
                        }
                    }
                    if (is_new_namespace) {
                        body_ok = semantic_analyzer_analyze_statement(analyzer, body_node);
                    }
                } else if (is_new_namespace) {
                    body_ok = semantic_analyzer_analyze_statement(analyzer, body_node);
                }
            }
            
            analyzer->symbol_table = prev_table;
            analyzer->symbol_table->current_scope = prev_scope;
            
            return body_ok;
        }

        case AST_CLASS_DECLARATION: {
            const char* class_name = stmt->data.class_decl.name;

            printf("[DEBUG SEM] Processing class: %s\n", class_name ? class_name : "NULL");
            
            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, class_name);
            if (existing && existing->state == SYMBOL_DEFINED) {
                semantic_analyzer_add_error(analyzer, "Class %s already declared", class_name);
                return false;
            }

            if (stmt->data.class_decl.template_params && stmt->data.class_decl.template_param_count > 0) {
                
                GenericParameter* params_list = NULL;
                for (int i = 0; i < stmt->data.class_decl.template_param_count; i++) {
                    generics_add_parameter(&params_list, stmt->data.class_decl.template_params[i]);
                }
                
                if (params_list) {
                    
                    bool registered = generics_register_type(analyzer->generic_registry, class_name, 
                                                           params_list, stmt->data.class_decl.template_param_count,
                                                           stmt); 
                    
                    if (!registered) {
                        generics_free_parameters(params_list); 
                        semantic_analyzer_add_error(analyzer, "Failed to register template class %s", class_name);
                        return false;
                    }
                }
            }

            SymbolEntry* class_sym = symbol_table_define(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0, NULL);
            if (!class_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define class %s", class_name);
                return false;
            }
            SymbolTable* prev_table = analyzer->symbol_table;
            if (!class_sym->nested_table) {
                class_sym->nested_table = symbol_table_create();
                if (class_sym->nested_table) {
                    class_sym->nested_table->parent_table = prev_table;
                }
            }

            analyzer->symbol_table = class_sym->nested_table;

            if (!analyzer->symbol_table->current_scope) {
                symbol_table_push_scope(analyzer->symbol_table);
            }

            SymbolEntry* self_class_symbol = symbol_table_define(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0, NULL);
            if (self_class_symbol) {
                self_class_symbol->nested_table = class_sym->nested_table;
            }

            semantic_analyzer_push_class_context(analyzer, class_name);

            bool body_ok = true;
            ASTNode* body_node = stmt->data.class_decl.body;
            
            printf("[DEBUG SEM] Class %s has body: %s, type=%d\n", 
                   class_name, 
                   body_node ? "YES" : "NO", 
                   body_node ? body_node->type : -1);
                   
            if (body_node && body_node->type == AST_BLOCK) {
                printf("[DEBUG SEM] Class %s has %d members\n", class_name, body_node->data.block.statement_count);
                
                for (int i = 0; i < body_node->data.block.statement_count && body_ok; i++) {
                    ASTNode* member = body_node->data.block.statements[i];
                    
                    printf("[DEBUG SEM] Class %s member[%d]: type=%d\n", class_name, i, member ? member->type : -1);
                    
                    if (!member) continue;
                    
                    if (member->type == AST_ACCESS_MODIFIER) {
                        member = member->data.access_modifier.member;
                        if (!member) continue;
                    }
                    
                    body_ok = semantic_analyzer_analyze_statement(analyzer, member);
                }
            } else if (body_node) {
                body_ok = semantic_analyzer_analyze_statement(analyzer, body_node);
            }

            semantic_analyzer_pop_class_context(analyzer);

            analyzer->symbol_table = prev_table;

            return body_ok;
        }

        case AST_TEMPLATE_DECLARATION:
            
            return semantic_analyzer_analyze_generic_decl(analyzer, stmt);

        case AST_SWITCH_STATEMENT: {
            bool cond_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.switch_stmt.expression);
            bool cases_ok = true;

            for (int i = 0; i < stmt->data.switch_stmt.case_count && cases_ok; i++) {
                cases_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.switch_stmt.cases[i]);
            }

            bool default_ok = true;
            if (stmt->data.switch_stmt.default_case) {
                default_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.switch_stmt.default_case);
            }

            return cond_ok && cases_ok && default_ok;
        }

        case AST_CASE_CLAUSE: {
            bool value_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.case_clause.value);
            bool stmts_ok = true;

            for (int i = 0; i < stmt->data.case_clause.statement_count && stmts_ok; i++) {
                stmts_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.case_clause.statements[i]);
            }

            return value_ok && stmts_ok;
        }

        case AST_DEFAULT_CLAUSE: {
            bool stmts_ok = true;

            for (int i = 0; i < stmt->data.default_clause.statement_count && stmts_ok; i++) {
                stmts_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.default_clause.statements[i]);
            }

            return stmts_ok;
        }

        case AST_BREAK_STATEMENT:
        case AST_CONTINUE_STATEMENT:
            return true;

        case AST_PRINT_STATEMENT: {
            bool all_ok = true;
            for (int i = 0; i < stmt->data.print_stmt.value_count; i++) {
                if (!semantic_analyzer_analyze_expression(analyzer, stmt->data.print_stmt.values[i])) {
                    all_ok = false;
                }
            }
            return all_ok;
        }

        case AST_UNSAFE_CALL: {
            int old_mode = analyzer->is_unsafe_mode;
            analyzer->is_unsafe_mode = 1;
            bool result = semantic_analyzer_analyze_expression(analyzer, stmt->data.unsafe_call.expression);
            analyzer->is_unsafe_mode = old_mode;
            return result;
        }

        case AST_ACCESS_MODIFIER: {

            if (stmt->data.access_modifier.member) {
                return semantic_analyzer_analyze_statement(analyzer, stmt->data.access_modifier.member);
            }
            return true;
        }

        case AST_FUNCTION_DECLARATION: {
            const char* func_name = stmt->data.function_decl.name;

            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, func_name);
            if (existing && existing->state == SYMBOL_DEFINED) {
                semantic_analyzer_add_error(analyzer, "Function %s already defined", func_name);
                return false;
            }

            SymbolEntry* func_sym = symbol_table_define(analyzer->symbol_table, func_name, SYMBOL_FUNCTION, 0, NULL);
            if (!func_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define function %s", func_name);
                return false;
            }
            func_sym->state = SYMBOL_DEFINED;

            if (strcmp(func_name, "main") == 0) {
                analyzer->has_entry_point = true;
                if (analyzer->entry_point_name) KRT_FREE(analyzer->entry_point_name);
                analyzer->entry_point_name = KRT_STRDUP(func_name);
                func_sym->is_entry_point = true;
            }

            if (stmt->data.function_decl.body) {
                symbol_table_push_scope(analyzer->symbol_table);
                for (int i = 0; i < stmt->data.function_decl.parameter_count; i++) {
                    SymbolEntry* param = symbol_table_define(analyzer->symbol_table, stmt->data.function_decl.parameters[i], SYMBOL_VARIABLE, 0, NULL);
                    if (param) {
                        param->value_type = stmt->data.function_decl.parameter_types[i];
                    }
                }
                bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.function_decl.body);
                symbol_table_pop_scope(analyzer->symbol_table);
                return body_ok;
            }
            return true;
        }

        case AST_STATIC_FUNCTION_DECLARATION: {
            const char* method_name = stmt->data.static_function_decl.name;

            printf("[DEBUG SEM] Processing static function: %s, params=%d\n", 
                   method_name ? method_name : "NULL", 
                   stmt->data.static_function_decl.parameter_count);

            const char* current_class = semantic_analyzer_get_current_class_context(analyzer);

            char* mangled_name = NULL;
            const char* function_name = method_name;
            if (current_class) {
                const char* ns_path[2] = { current_class, NULL };
                mangled_name = name_mangle_function(ns_path, method_name,
                                                      stmt->data.static_function_decl.parameter_types,
                                                      stmt->data.static_function_decl.parameter_count);
                if (mangled_name) {
                    function_name = mangled_name;
                }
            }

            SymbolEntry* existing = symbol_table_lookup_scope_chain(analyzer->symbol_table, function_name);
            if (existing && existing->state == SYMBOL_DEFINED) {
                semantic_analyzer_add_error(analyzer, "Static method %s already defined", method_name);
                if (mangled_name) {
                    KRT_FREE(mangled_name);
                }
                return false;
            }

            SymbolEntry* func_sym = symbol_table_define(analyzer->symbol_table, function_name, SYMBOL_FUNCTION, 0, NULL);
            if (!func_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define static method %s", method_name);
                if (mangled_name) {
                    KRT_FREE(mangled_name);
                }
                return false;
            }
            func_sym->state = SYMBOL_DEFINED;

            symbol_table_push_scope(analyzer->symbol_table);
            printf("[DEBUG SEM] Pushed scope for function %s\n", method_name);
            
            for (int i = 0; i < stmt->data.static_function_decl.parameter_count; i++) {
                const char* param_name = stmt->data.static_function_decl.parameters[i];
                printf("[DEBUG SEM] Registering parameter[%d]: %s\n", i, param_name ? param_name : "NULL");
                
                SymbolEntry* param = symbol_table_define(analyzer->symbol_table, param_name, SYMBOL_VARIABLE, 0, NULL);
                if (param) {
                    param->value_type = stmt->data.static_function_decl.parameter_types[i];
                    printf("[DEBUG SEM] Parameter %s registered successfully\n", param_name);
                } else {
                    printf("[DEBUG SEM] FAILED to register parameter %s\n", param_name);
                }
            }
            
            printf("[DEBUG SEM] About to analyze body of function %s\n", method_name);
            bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.static_function_decl.body);
            printf("[DEBUG SEM] Body analysis for %s: %s\n", method_name, body_ok ? "SUCCESS" : "FAILED");
            
            symbol_table_pop_scope(analyzer->symbol_table);

            if (mangled_name) {
                KRT_FREE(mangled_name);
            }
            return body_ok;
        }

        case AST_CONSTRUCTOR_DECLARATION: {
            const char* current_class = semantic_analyzer_get_current_class_context(analyzer);
            if (!current_class) {
                return false;
            }

            int parameter_count = stmt->data.constructor_decl.parameter_count;
            char* mangled_name = name_mangle_constructor(current_class, parameter_count);
            if (!mangled_name) {
                return false;
            }

            SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, mangled_name);
            if (existing && existing->state == SYMBOL_DEFINED) {
                semantic_analyzer_add_error(analyzer, "Constructor %s already defined", current_class);
                KRT_FREE(mangled_name);
                return false;
            }

            SymbolEntry* ctor_sym = symbol_table_define(analyzer->symbol_table, mangled_name, SYMBOL_FUNCTION, 0, NULL);
            if (!ctor_sym) {
                semantic_analyzer_add_error(analyzer, "Failed to define constructor %s", current_class);
                KRT_FREE(mangled_name);
                return false;
            }
            ctor_sym->state = SYMBOL_DEFINED;
            KRT_FREE(mangled_name);

            if (stmt->data.constructor_decl.body) {
                symbol_table_push_scope(analyzer->symbol_table);
                for (int i = 0; i < stmt->data.constructor_decl.parameter_count; i++) {
                    SymbolEntry* param = symbol_table_define(analyzer->symbol_table, stmt->data.constructor_decl.parameters[i], SYMBOL_VARIABLE, 0, NULL);
                    if (param) {
                        param->value_type = stmt->data.constructor_decl.parameter_types[i];
                    }
                }

                bool base_ok = true;
                for (int i = 0; i < stmt->data.constructor_decl.base_argument_count && base_ok; i++) {
                    base_ok = semantic_analyzer_analyze_expression(analyzer, stmt->data.constructor_decl.base_arguments[i]);
                }

                bool body_ok = semantic_analyzer_analyze_statement(analyzer, stmt->data.constructor_decl.body);
                symbol_table_pop_scope(analyzer->symbol_table);
                return base_ok && body_ok;
            }
            return true;
        }

        case AST_BLOCK: {
            symbol_table_push_scope(analyzer->symbol_table);
            bool overall_result = true;
            for (int i = 0; i < stmt->data.block.statement_count; i++) {
                bool stmt_result = semantic_analyzer_analyze_statement(analyzer, stmt->data.block.statements[i]);
                if (!stmt_result) {
                    overall_result = false;
                }
            }
            symbol_table_pop_scope(analyzer->symbol_table);
            return overall_result;
        }

        case AST_BINARY_OPERATION:
        case AST_UNARY_OPERATION:
        case AST_TERNARY_OPERATION:
        case AST_CALL:
        case AST_IDENTIFIER:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:

            return semantic_analyzer_analyze_expression(analyzer, stmt);

        default:
            return true;
    }
}

FunctionAnalysisResult semantic_analyzer_analyze_function(SemanticAnalyzer* analyzer, ASTNode* function_node) {
    FunctionAnalysisResult result = {0};
    result.success = false;

    if (!analyzer || !function_node || (function_node->type != AST_FUNCTION_DECLARATION && function_node->type != AST_STATIC_FUNCTION_DECLARATION)) {
        return result;
    }

    const char* func_name = (function_node->type == AST_FUNCTION_DECLARATION) ? 
                            function_node->data.function_decl.name : 
                            function_node->data.static_function_decl.name;

    SymbolEntry* existing = symbol_table_lookup(analyzer->symbol_table, func_name);
    if (existing && existing->state == SYMBOL_DEFINED) {
        semantic_analyzer_add_error(analyzer, "Function %s already defined", func_name);
        return result;
    }

    SymbolEntry* func_symbol = symbol_table_define(analyzer->symbol_table, func_name,
                                                  SYMBOL_FUNCTION, 0, NULL);
    if (!func_symbol) {
        semantic_analyzer_add_error(analyzer, "Failed to define function %s", func_name);
        return result;
    }
    func_symbol->state = SYMBOL_DEFINED;

    if (strcmp(func_name, "main") == 0) {
        analyzer->has_entry_point = true;
        if (analyzer->entry_point_name) KRT_FREE(analyzer->entry_point_name);
        analyzer->entry_point_name = KRT_STRDUP(func_name);
        func_symbol->is_entry_point = true;
    }

    symbol_table_push_scope(analyzer->symbol_table);

    int param_count = (function_node->type == AST_FUNCTION_DECLARATION) ? 
                      function_node->data.function_decl.parameter_count : 
                      function_node->data.static_function_decl.parameter_count;
    for (int i = 0; i < param_count; i++) {
        const char* param_name = (function_node->type == AST_FUNCTION_DECLARATION) ? 
                                 function_node->data.function_decl.parameters[i] : 
                                 function_node->data.static_function_decl.parameters[i];
        KrtTokenType param_type = (function_node->type == AST_FUNCTION_DECLARATION) ?
                                   function_node->data.function_decl.parameter_types[i] :
                                   function_node->data.static_function_decl.parameter_types[i];
        SymbolEntry* param_symbol = symbol_table_define(analyzer->symbol_table, param_name,
                           SYMBOL_VARIABLE, 0, NULL);
        if (param_symbol) {
            param_symbol->value_type = param_type;
            int is_array = (function_node->type == AST_FUNCTION_DECLARATION) ?
                           (function_node->data.function_decl.parameter_is_array ? function_node->data.function_decl.parameter_is_array[i] : 0) :
                           (function_node->data.static_function_decl.parameter_is_array ? function_node->data.static_function_decl.parameter_is_array[i] : 0);
            if (is_array) {
                param_symbol->is_array = 1;
            }
        }
    }

    ASTNode* body = (function_node->type == AST_FUNCTION_DECLARATION) ? 
                    function_node->data.function_decl.body : 
                    function_node->data.static_function_decl.body;
    
    bool body_ok = semantic_analyzer_analyze_statement(analyzer, body);

    symbol_table_pop_scope(analyzer->symbol_table);

    result.success = body_ok;
    result.param_count = param_count;
    result.function_symbol = func_symbol;
    result.has_return_statement = true;

    return result;
}

bool semantic_analyzer_analyze_generic_decl(SemanticAnalyzer* analyzer, ASTNode* generic_decl) {
    if (!analyzer || !generic_decl || generic_decl->type != AST_TEMPLATE_DECLARATION) {
        return false;
    }
    
    ASTNode* declaration = generic_decl->data.template_decl.declaration;
    if (!declaration) {
        semantic_analyzer_add_error(analyzer, "Template declaration missing declaration");
        return false;
    }
    
    const char* type_name = NULL;
    if (declaration->type == AST_CLASS_DECLARATION) {
        type_name = declaration->data.class_decl.name;
    } else if (declaration->type == AST_FUNCTION_DECLARATION) {
        type_name = declaration->data.function_decl.name;
    }
    
    if (!type_name) {
        semantic_analyzer_add_error(analyzer, "Template declaration missing name");
        return false;
    }
    
    GenericParameter* params = NULL;
    int param_count = 0;
    
    if (generic_decl->data.template_decl.parameters) {
        
        for (int i = 0; i < generic_decl->data.template_decl.parameter_count; i++) {
            ASTNode* param_node = generic_decl->data.template_decl.parameters[i];
            if (param_node && param_node->type == AST_TEMPLATE_PARAMETER) {
                const char* param_name = param_node->data.template_param.param_name;
                if (param_name) {
                    generics_add_parameter(&params, param_name);
                    param_count++;
                }
            }
        }
    }
    
    bool success = generics_register_type(analyzer->generic_registry, type_name, 
                                          params, param_count, declaration);
    
    if (!success) {
        semantic_analyzer_add_error(analyzer, "Failed to register generic type: %s", type_name);
        generics_free_parameters(params);
        return false;
    }
    
    return true;
}

bool semantic_analyzer_analyze_generic_instantiation(SemanticAnalyzer* analyzer, 
                                                       const char* base_type, 
                                                       const char** type_args, 
                                                       int arg_count) {
    if (!analyzer || !base_type || !type_args || arg_count <= 0) {
        return false;
    }
    
    if (!generics_is_generic_type(analyzer->generic_registry, base_type)) {
        semantic_analyzer_add_error(analyzer, "Unknown generic type: %s", base_type);
        return false;
    }
    
    GenericTypeInfo* type_info = generics_get_type_info(analyzer->generic_registry, base_type);
    
    ASTNode* declaration = NULL;
    if (type_info && type_info->declaration) {
        declaration = type_info->declaration;
    } else {
        
        if (type_info) {
            KRT_FREE(type_info);
            return true; 
        } else {
            semantic_analyzer_add_error(analyzer, "Unknown generic type: %s", base_type);
            if (type_info) {
                KRT_FREE(type_info);
            }
            return false;
        }
    }
    
    if (!declaration || declaration->type != AST_CLASS_DECLARATION) {
        semantic_analyzer_add_error(analyzer, "Invalid template declaration for: %s", base_type);
        if (type_info) {
            KRT_FREE(type_info);
        }
        return false;
    }
    
    if (!semantic_analyzer_validate_constraints(analyzer, declaration, type_args, arg_count)) {
        semantic_analyzer_add_error(analyzer, "Constraint validation failed for generic type: %s", base_type);
        KRT_FREE(type_info);
        return false;
    }
    
    KRT_FREE(type_info);
    
    bool success = generics_instantiate_type(analyzer->generic_registry, base_type,
                                          type_args, arg_count, analyzer->symbol_table);
    
    if (!success) {
        semantic_analyzer_add_error(analyzer, "Failed to instantiate generic type: %s", base_type);
        return false;
    }
    
    return true;
}

bool semantic_analyzer_validate_constraints(SemanticAnalyzer* analyzer, ASTNode* class_decl, const char** type_args, int arg_count) {
    if (!analyzer || !class_decl || class_decl->type != AST_CLASS_DECLARATION) {
        return false;
    }
    
    if (class_decl->data.class_decl.constraint_count == 0) {
        return true; 
    }
    
    char** template_params = class_decl->data.class_decl.template_params;
    int template_param_count = class_decl->data.class_decl.template_param_count;
    
    if (!template_params || template_param_count == 0) {
        semantic_analyzer_add_error(analyzer, "Class has constraints but no template parameters");
        return false;
    }
    
    if (arg_count != template_param_count) {
        semantic_analyzer_add_error(analyzer, "Wrong number of type arguments: expected %d, got %d", 
                                   template_param_count, arg_count);
        return false;
    }
    
    for (int param_idx = 0; param_idx < template_param_count; param_idx++) {
        const char* param_name = template_params[param_idx];
        bool has_class_constraint = false;
        bool has_struct_constraint = false;
        
        for (int i = 0; i < class_decl->data.class_decl.constraint_count; i++) {
            ASTNode* constraint = class_decl->data.class_decl.constraints[i];
            if (!constraint || constraint->type != AST_GENERIC_CONSTRAINT) {
                continue;
            }
            
            if (strcmp(constraint->data.generic_constraint.param_name, param_name) == 0) {
                const char* constraint_type = constraint->data.generic_constraint.constraint_type;
                if (strcmp(constraint_type, "class") == 0) {
                    has_class_constraint = true;
                } else if (strcmp(constraint_type, "struct") == 0) {
                    has_struct_constraint = true;
                }
            }
        }
        
        if (has_class_constraint && has_struct_constraint) {
            semantic_analyzer_add_error(analyzer, "Parameter '%s' has conflicting class and struct constraints", param_name);
            return false;
        }
    }
    
    for (int i = 0; i < class_decl->data.class_decl.constraint_count; i++) {
        ASTNode* constraint = class_decl->data.class_decl.constraints[i];
        if (!constraint || constraint->type != AST_GENERIC_CONSTRAINT) {
            continue;
        }
        
        const char* param_name = constraint->data.generic_constraint.param_name;
        const char* constraint_type = constraint->data.generic_constraint.constraint_type;
        
        if (!param_name || !constraint_type) {
            semantic_analyzer_add_error(analyzer, "Invalid constraint: missing parameter or type");
            return false;
        }
        
        int param_index = -1;
        for (int j = 0; j < template_param_count; j++) {
            if (strcmp(template_params[j], param_name) == 0) {
                param_index = j;
                break;
            }
        }
        
        if (param_index == -1) {
            semantic_analyzer_add_error(analyzer, "Constraint parameter '%s' not found in template parameters", param_name);
            return false;
        }
        
        const char* type_arg = type_args[param_index];
        
        if (strcmp(constraint_type, "class") == 0) {
            
            if (strcmp(type_arg, "int") == 0 || strcmp(type_arg, "float") == 0 || 
                strcmp(type_arg, "string") == 0 || strcmp(type_arg, "bool") == 0) {
                semantic_analyzer_add_error(analyzer, "Type argument '%s' for parameter '%s' violates class constraint (must be reference type)", 
                                           type_arg, param_name);
                return false;
            }
        }
        else if (strcmp(constraint_type, "struct") == 0) {
            
            if (strcmp(type_arg, "int") != 0 && strcmp(type_arg, "float") != 0 && 
                strcmp(type_arg, "bool") != 0 && strcmp(type_arg, "char") != 0) {
                semantic_analyzer_add_error(analyzer, "Type argument '%s' for parameter '%s' violates struct constraint (must be value type)", 
                                           type_arg, param_name);
                return false;
            }
        }
        else if (strcmp(constraint_type, "new()") == 0) {
            continue;
        }
        else {
            
            if (strcmp(constraint_type, "IDisposable") == 0 && strcmp(type_arg, "string") == 0) {
                semantic_analyzer_add_error(analyzer, "Type argument '%s' for parameter '%s' does not implement interface '%s'", 
                                           type_arg, param_name, constraint_type);
                return false;
            }
            
            continue;
        }
    }
    
    return true;
}

SemanticAnalysisResult* semantic_analyzer_analyze(SemanticAnalyzer* analyzer, ASTNode* ast) {
    if (!analyzer || !ast) {
        return NULL;
    }

    printf("[DEBUG SEM] Starting semantic analysis, AST type=%d\n", ast->type);
    
    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {
        printf("[DEBUG SEM] AST has %d statements\n", ast->data.block.statement_count);
        for (int i = 0; i < ast->data.block.statement_count && i < 10; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            printf("[DEBUG SEM] Statement[%d]: type=%d\n", i, stmt ? stmt->type : -1);
        }
    }

    SemanticAnalysisResult* result = (SemanticAnalysisResult*)KRT_MALLOC(sizeof(SemanticAnalysisResult));
    if (!result) {
        return NULL;
    }

    result->success = true;
    result->error_count = 0;
    result->warning_count = 0;
    result->symbol_table = analyzer->symbol_table;
    result->error_messages = NULL;

    analyzer->error_count = 0;
    analyzer->warning_count = 0;
    
    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && decl->type == AST_USING_DIRECTIVE) {
                if (!semantic_analyzer_analyze_statement(analyzer, decl)) {
                    result->success = false;
                }
            }
        }
        
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && decl->type == AST_NAMESPACE_DECLARATION) {
                if (!semantic_analyzer_analyze_statement(analyzer, decl)) {
                    result->success = false;
                }
            }
        }
        
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && decl->type == AST_CLASS_DECLARATION) {
                const char* class_name = decl->data.class_decl.name;
                SymbolEntry* existing = symbol_table_lookup_current_scope(analyzer->symbol_table, class_name);
                if (!existing) {
                    SymbolEntry* class_sym = symbol_table_declare(analyzer->symbol_table, class_name, SYMBOL_CLASS, 0);
                    if (class_sym) {
                        class_sym->state = SYMBOL_FORWARD_REF;
                        if (!class_sym->nested_table) {
                            class_sym->nested_table = symbol_table_create();
                        }
                    }
                } else if (existing->state == SYMBOL_FORWARD_REF) {
                    semantic_analyzer_add_error(analyzer, "Class %s already declared", class_name);
                }
            }
        }

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && (decl->type == AST_CLASS_DECLARATION || decl->type == AST_TEMPLATE_DECLARATION)) {
                if (!semantic_analyzer_analyze_statement(analyzer, decl)) {
                    result->success = false;
                }
            }
        }
        
        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && decl->type == AST_STATIC_VARIABLE_DECLARATION) {
                if (!semantic_analyzer_analyze_static_variable_decl(analyzer, decl)) {
                    result->success = false;
                }
            }
        }

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* decl = ast->data.block.statements[i];
            if (decl && (decl->type == AST_FUNCTION_DECLARATION || decl->type == AST_STATIC_FUNCTION_DECLARATION)) {
                FunctionAnalysisResult func_result = semantic_analyzer_analyze_function(analyzer, decl);
                if (!func_result.success) {
                    result->success = false;
                }
            }
        }

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt && stmt->type != AST_FUNCTION_DECLARATION &&
                stmt->type != AST_STATIC_FUNCTION_DECLARATION &&
                stmt->type != AST_CLASS_DECLARATION &&
                stmt->type != AST_TEMPLATE_DECLARATION &&
                stmt->type != AST_STATIC_VARIABLE_DECLARATION &&
                stmt->type != AST_NAMESPACE_DECLARATION &&
                stmt->type != AST_USING_DIRECTIVE) {
                if (!semantic_analyzer_analyze_statement(analyzer, stmt)) {
                    result->success = false;
                }
            }
        }

        if (analyzer->global_symbol_table) {
            analyzer->symbol_table = analyzer->global_symbol_table;
        }
    } else if (ast->type == AST_FUNCTION_DECLARATION || ast->type == AST_STATIC_FUNCTION_DECLARATION) {

        FunctionAnalysisResult func_result = semantic_analyzer_analyze_function(analyzer, ast);
        if (!func_result.success) {
            result->success = false;
        }
    } else if (ast->type == AST_BLOCK) {

        for (int i = 0; i < ast->data.block.statement_count; i++) {
            ASTNode* stmt = ast->data.block.statements[i];
            if (stmt && !semantic_analyzer_analyze_statement(analyzer, stmt)) {
                result->success = false;
            }
        }
    } else {

        if (!semantic_analyzer_analyze_statement(analyzer, ast)) {
            result->success = false;
        }
    }

    symbol_table_check_undefined(analyzer->symbol_table);

    if (!analyzer->has_entry_point) {
        SymbolEntry* main_sym = symbol_table_lookup(analyzer->symbol_table, "main");
        if (main_sym && main_sym->type == SYMBOL_FUNCTION) {
            analyzer->has_entry_point = true;
        }
    }

    if (!analyzer->has_entry_point) {
        semantic_analyzer_add_warning(analyzer, "No entry point (main function) defined. The program will not have a valid starting point for execution.");
    }

    result->error_count = analyzer->error_count;
    result->warning_count = analyzer->warning_count;

    if (result->error_count > 0) {
        result->success = false;
    } else {
        result->success = true;
    }

    return result;
}