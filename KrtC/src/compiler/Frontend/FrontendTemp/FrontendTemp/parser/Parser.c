
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int strcmp(const char* s1, const char* s2);
#include "../../../../../Accelerator.h"
#include "../../../../../Core/Utils/KrtCommon.h"
#include "../../../../../Core/Memory/Arena.h"
#include "Parser.h"
#include "../Lexer/Tokenizer.h"

#define PARSER_DEFAULT_ARENA_SIZE (128 * 1024)
#define PARSER_FUNCTION_CAPACITY_INIT 16
#define PARSER_ATTRIBUTE_CAPACITY_INIT 4
#define PARSER_PARAMETER_CAPACITY_INIT 8
#define PARSER_ARGUMENT_CAPACITY_INIT 8
#define PARSER_ELEMENT_CAPACITY_INIT 8
#define PARSER_MEMBER_CAPACITY_INIT 8

#define PARSER_LIKELY(x) (x)
#define PARSER_UNLIKELY(x) (x)

#define PARSER_MALLOC(size) KRT_MALLOC(size)
#define PARSER_REALLOC(ptr, size) KRT_REALLOC(ptr, size)
#define PARSER_FREE(ptr) KRT_FREE(ptr)

#define PARSER_ALLOC_FROM_ARENA(parser, size) KrtArenaAlloc((parser)->arena, size)

#define PARSER_CLEANUP_AND_RETURN_NULL(cleanup_expr) \
    do {                                               \
        cleanup_expr;                                  \
        return NULL;                                   \
    } while (0)

#define PARSER_FREE_STRING_ARRAY(arr, count)           \
    do {                                               \
        if (arr) {                                     \
            for (int i = 0; i < count; i++) {          \
                if (arr[i]) {                          \
                    KRT_FREE(arr[i]);                  \
                }                                      \
            }                                          \
            KRT_FREE(arr);                             \
        }                                              \
    } while (0)

#define PARSER_FREE_AST_NODE_ARRAY(arr, count)         \
    do {                                               \
        if (arr) {                                     \
            for (int i = 0; i < count; i++) {          \
                if (arr[i]) {                          \
                    ast_destroy_node(arr[i]);          \
                }                                      \
            }                                          \
            KRT_FREE(arr);                             \
        }                                              \
    } while (0)

#define PARSER_CREATE_NODE(type, line, col) ast_create_node_arena(type, line, col, parser->arena)
#define PARSER_STRDUP(s) arena_strdup(parser->arena, s)

// Redefine ast_create_node to use parser's arena for consistent memory allocation
// This ensures all AST nodes in Parser.c use arena allocation
#undef ast_create_node
#define ast_create_node(type, line, col) ast_create_node_arena(type, line, col, parser->arena)

static char* arena_strdup(KrtArena* arena, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* result = (char*)KrtArenaAlloc(arena, len);
    if (result) {
        memcpy(result, str, len);
    }
    return result;
}

static void parser_advance(Parser* parser) {
    token_free(&parser->current_token);
    parser->current_token = lexer_next_token(parser->lexer);
}

static ASTNode* parser_parse_expression(Parser* parser);
static ASTNode* parser_parse_statement(Parser* parser);
static ASTNode* parser_parse_block(Parser* parser);
static ASTNode* parser_parse_new_expression(Parser* parser);
static ASTNode* parser_parse_postfix_expression(Parser* parser);
static ASTNode* parser_parse_primary_debug(Parser* parser);
static ASTNode* parser_parse_constructor_declaration(Parser* parser);
static ASTNode* parser_parse_destructor_declaration(Parser* parser);
static ASTNode* parser_parse_static_member_declaration(Parser* parser);

static ASTNode* parser_parse_function_declaration(Parser* parser);
static ASTNode* parser_parse_ternary_operation(Parser* parser);
static ASTNode* parser_parse_type_declaration(Parser* parser);
static ASTNode* parser_parse_typed_function_declaration(Parser* parser, KrtTokenType return_type, char* function_name, int is_static, int line, int col);
static int parser_is_type_keyword(KrtTokenType type);
static int parser_parse_parameter_list(Parser* parser, char*** parameters, KrtTokenType** parameter_types, int** parameter_is_params, int** parameter_is_array, ASTNode*** parameter_default_values, int* parameter_count);
static void parser_free_parameter_list(char** parameters, KrtTokenType* parameter_types, int* parameter_is_params, int* parameter_is_array, ASTNode** parameter_default_values, int parameter_count);
static ASTNode* parser_create_function_node(Parser* parser, int is_static, int is_async, char* name, char** parameters, KrtTokenType* parameter_types, int* parameter_is_params, int* parameter_is_array, ASTNode** parameter_default_values, int parameter_count, ASTNode* body, KrtTokenType return_type, int line, int col);
static ASTNode* parser_parse_property_declaration(Parser* parser, KrtTokenType type, char* name, ASTNode** attributes, int attribute_count);
static ASTNode* parser_parse_lambda_expression(Parser* parser, char** parameters, int parameter_count);
static ASTNode* parser_parse_linq_query(Parser* parser);
static ASTNode* parser_parse_match_expression(Parser* parser);
static ASTNode* parser_parse_pattern(Parser* parser);
static int parser_parse_lambda_parameters(Parser* parser, char*** parameters, int* parameter_count);
static ASTNode* parser_parse_fixed_statement(Parser* parser);
static ASTNode* parser_parse_stackalloc_expression(Parser* parser);
static ASTNode* parser_parse_await_expression(Parser* parser);
static ASTNode* parser_parse_generic_constraints(Parser* parser);
static void parser_add_declared_function(Parser* parser, const char* func_name) {
    if (!parser || !func_name) {
        return;
    }
    for (int i = 0; i < parser->declared_function_count; i++) {
        if (strcmp(parser->declared_functions[i], func_name) == 0) {
            return;
        }
    }
    if (parser->declared_function_count >= parser->declared_function_capacity) {
        int new_capacity = parser->declared_function_capacity == 0 ? PARSER_FUNCTION_CAPACITY_INIT : parser->declared_function_capacity * 2;
        char** new_functions = (char**)KRT_REALLOC(parser->declared_functions, new_capacity * sizeof(char*));
        if (!new_functions) {
            return;
        }
        parser->declared_functions = new_functions;
        parser->declared_function_capacity = new_capacity;
    }
    parser->declared_functions[parser->declared_function_count++] = KRT_STRDUP(func_name);
}

Parser* parser_create(Lexer* lexer) {
    return parser_create_with_arena(lexer, PARSER_DEFAULT_ARENA_SIZE);
}

Parser* parser_create_with_arena(Lexer* lexer, size_t arena_size) {
    if (!lexer) {
        return NULL;
    }
    Parser* parser = (Parser*)KRT_MALLOC(sizeof(Parser));
    if (!parser) {
        return NULL;
    }
    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
    parser->declared_functions = NULL;
    parser->declared_function_count = 0;
    parser->declared_function_capacity = 0;
    parser->is_unsafe_mode = 0;
    parser->current_class = NULL;
    
    if (arena_size > 0) {
        parser->arena = KrtArenaCreate(arena_size);
        if (!parser->arena) {
            KRT_FREE(parser);
            return NULL;
        }
    } else {
        parser->arena = NULL;
    }
    
    return parser;
}

void parser_destroy(Parser* parser) {
    if (!parser) {
        return;
    }
    if (parser->declared_functions) {
        for (int i = 0; i < parser->declared_function_count; i++) {
            KRT_FREE(parser->declared_functions[i]);
        }
        KRT_FREE(parser->declared_functions);
    }
    if (parser->arena) {
        KrtArenaDestroy(parser->arena);
    }
    KRT_FREE(parser);
}

static int parser_is_type_keyword(KrtTokenType type) {
    switch (type) {
        case TOKEN_INT8:
        case TOKEN_INT16:
        case TOKEN_INT32:
        case TOKEN_INT64:
        case TOKEN_UINT8:
        case TOKEN_UINT16:
        case TOKEN_UINT32:
        case TOKEN_UINT64:
        case TOKEN_FLOAT32:
        case TOKEN_FLOAT64:
        case TOKEN_BOOL:
        case TOKEN_TYPE_STRING:
        case TOKEN_CHAR:
        case TOKEN_VOID:
            return 1;
        default:
            return 0;
    }
}

static void parser_free_parameter_list(char** parameters,
                                       KrtTokenType* parameter_types,
                                       int* parameter_is_params,
                                       int* parameter_is_array,
                                       ASTNode** parameter_default_values,
                                       int parameter_count) {
    if (parameters) {
        for (int i = 0; i < parameter_count; i++) {
            if (parameters[i]) {
                KRT_FREE(parameters[i]);
            }
        }
        KRT_FREE(parameters);
    }
    if (parameter_types) {
        KRT_FREE(parameter_types);
    }
    if (parameter_is_params) {
        KRT_FREE(parameter_is_params);
    }
    if (parameter_is_array) {
        KRT_FREE(parameter_is_array);
    }
    if (parameter_default_values) {
        for (int i = 0; i < parameter_count; i++) {
            if (parameter_default_values[i]) {
                ast_destroy_node(parameter_default_values[i]);
            }
        }
        KRT_FREE(parameter_default_values);
    }
}

static int parser_parse_parameter_list(Parser* parser, char*** parameters, KrtTokenType** parameter_types, int** parameter_is_params, int** parameter_is_array, ASTNode*** parameter_default_values, int* parameter_count) {
    if (!parameters || !parameter_types || !parameter_is_params || !parameter_is_array || !parameter_default_values || !parameter_count) {
        return 0;
    }
    *parameters = NULL;
    *parameter_types = NULL;
    *parameter_is_params = NULL;
    *parameter_is_array = NULL;
    *parameter_default_values = NULL;
    *parameter_count = 0;
    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
        return 1;
    }
    while (1) {
        int is_params = 0;
        if (parser->current_token.type == TOKEN_PARAMS) {
            is_params = 1;
            parser_advance(parser);
        }
        KrtTokenType param_type = TOKEN_UNKNOWN;
        int param_is_nullable = 0;
        int param_is_array = 0;
        if (parser_is_type_keyword(parser->current_token.type)) {
            param_type = parser->current_token.type;
            parser_advance(parser);
            if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
                parser_advance(parser);
                if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                    return 0;
                }
                parser_advance(parser);
                param_is_array = 1;
            }
            if (parser->current_token.type == TOKEN_QUESTION) {
                param_is_nullable = 1;
                parser_advance(parser);
            }
            (void)param_is_nullable;
        }
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_is_params = NULL;
            *parameter_is_array = NULL;
            *parameter_default_values = NULL;
            *parameter_count = 0;
            return 0;
        }
        char* param_name = KRT_STRDUP(parser->current_token.value);
        if (!param_name) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_is_params = NULL;
            *parameter_is_array = NULL;
            *parameter_default_values = NULL;
            *parameter_count = 0;
            return 0;
        }

        ASTNode* default_value = NULL;
        parser_advance(parser);

        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (parser_is_type_keyword(parser->current_token.type)) {
                param_type = parser->current_token.type;
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
                param_type = TOKEN_UNKNOWN;
                parser_advance(parser);
            } else {
                KRT_FREE(param_name);
                parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
                *parameters = NULL;
                *parameter_types = NULL;
                *parameter_is_params = NULL;
                *parameter_is_array = NULL;
                *parameter_default_values = NULL;
                *parameter_count = 0;
                return 0;
            }
            if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
                parser_advance(parser);
                if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                    KRT_FREE(param_name);
                    parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
                    *parameters = NULL;
                    *parameter_types = NULL;
                    *parameter_is_params = NULL;
                    *parameter_is_array = NULL;
                    *parameter_default_values = NULL;
                    *parameter_count = 0;
                    return 0;
                }
                parser_advance(parser);
                param_is_array = 1;
            }
            if (parser->current_token.type == TOKEN_QUESTION) {
                parser_advance(parser);
            }
        }

        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            default_value = parser_parse_expression(parser);
            if (!default_value) {
                KRT_FREE(param_name);
                parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
                *parameters = NULL;
                *parameter_types = NULL;
                *parameter_is_params = NULL;
                *parameter_is_array = NULL;
                *parameter_default_values = NULL;
                *parameter_count = 0;
                return 0;
            }
        }

        char** new_parameters = (char**)KRT_REALLOC(*parameters, (*parameter_count + 1) * sizeof(char*));
        KrtTokenType* new_param_types = (KrtTokenType*)KRT_REALLOC(*parameter_types, (*parameter_count + 1) * sizeof(KrtTokenType));
        int* new_is_params = (int*)KRT_REALLOC(*parameter_is_params, (*parameter_count + 1) * sizeof(int));
        int* new_is_array = (int*)KRT_REALLOC(*parameter_is_array, (*parameter_count + 1) * sizeof(int));
        ASTNode** new_default_values = (ASTNode**)KRT_REALLOC(*parameter_default_values, (*parameter_count + 1) * sizeof(ASTNode*));
        if (!new_parameters || !new_param_types || !new_is_params || !new_is_array || !new_default_values) {
            KRT_FREE(param_name);
            if (default_value) ast_destroy_node(default_value);
            if (new_parameters) {
                *parameters = new_parameters;
            }
            if (new_param_types) {
                *parameter_types = new_param_types;
            }
            if (new_is_params) {
                *parameter_is_params = new_is_params;
            }
            if (new_is_array) {
                *parameter_is_array = new_is_array;
            }
            if (new_default_values) {
                *parameter_default_values = new_default_values;
            }
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_is_params = NULL;
            *parameter_is_array = NULL;
            *parameter_default_values = NULL;
            *parameter_count = 0;
            return 0;
        }
        *parameters = new_parameters;
        *parameter_types = new_param_types;
        *parameter_is_params = new_is_params;
        *parameter_is_array = new_is_array;
        *parameter_default_values = new_default_values;
        (*parameters)[*parameter_count] = param_name;
        (*parameter_types)[*parameter_count] = param_type;
        (*parameter_is_params)[*parameter_count] = is_params;
        (*parameter_is_array)[*parameter_count] = param_is_array;
        (*parameter_default_values)[*parameter_count] = default_value;
        (*parameter_count)++;

        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type) && parser->current_token.type != TOKEN_IDENTIFIER) {
                parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
                return 0;
            }
            (*parameter_types)[*parameter_count - 1] = parser->current_token.type;
            parser_advance(parser);
        }
        if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
            break;
        }
        if (parser->current_token.type != TOKEN_COMMA) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_is_params, *parameter_is_array, *parameter_default_values, *parameter_count);
            return 0;
        }
        parser_advance(parser);
    }
    return 1;
}

static ASTNode* parser_create_function_node(Parser* parser,
                                            int is_static,
                                            int is_async,
                                            char* name,
                                            char** parameters,
                                            KrtTokenType* parameter_types,
                                            int* parameter_is_params,
                                            int* parameter_is_array,
                                            ASTNode** parameter_default_values,
                                            int parameter_count,
                                            ASTNode* body,
                                            KrtTokenType return_type,
                                            int line,
                                            int col) {
    ASTNode* node = ast_create_node_arena(is_static ? AST_STATIC_FUNCTION_DECLARATION
                                              : AST_FUNCTION_DECLARATION, line, col, parser->arena);
    if (!node) {
        return NULL;
    }
    if (is_static) {
        node->data.static_function_decl.name = name;
        node->data.static_function_decl.parameters = parameters;
        node->data.static_function_decl.parameter_types = parameter_types;
        node->data.static_function_decl.parameter_is_params = parameter_is_params;
        node->data.static_function_decl.parameter_is_array = parameter_is_array;
        node->data.static_function_decl.parameter_default_values = parameter_default_values;
        node->data.static_function_decl.parameter_count = parameter_count;
        node->data.static_function_decl.body = body;
        node->data.static_function_decl.return_type = return_type;
    } else {
        node->data.function_decl.name = name;
        node->data.function_decl.parameters = parameters;
        node->data.function_decl.parameter_types = parameter_types;
        node->data.function_decl.parameter_is_params = parameter_is_params;
        node->data.function_decl.parameter_is_array = parameter_is_array;
        node->data.function_decl.parameter_default_values = parameter_default_values;
        node->data.function_decl.parameter_count = parameter_count;
        node->data.function_decl.body = body;
        node->data.function_decl.return_type = return_type;
        node->data.function_decl.is_async = is_async;
    }
    parser_add_declared_function(parser, name);
    return node;
}

static ASTNode* parser_parse_call(Parser* parser, ASTNode* callee) {
    parser_advance(parser);

    ASTNode** arguments = NULL;
    char** argument_names = NULL;
    int argument_count = 0;
    int capacity = 0;

    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        capacity = PARSER_ARGUMENT_CAPACITY_INIT;
        arguments = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
        argument_names = (char**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(char*));
        if (PARSER_UNLIKELY(!arguments || !argument_names)) {
            ast_destroy_node(callee);
            return NULL;
        }

        while (1) {
            char* param_name = NULL;
            ASTNode* arg = NULL;

            if (parser->current_token.type == TOKEN_IDENTIFIER) {
                Token next = lexer_peek_token(parser->lexer);
                KrtTokenType next_type = next.type;
                token_free(&next);

                if (next_type == TOKEN_COLON) {
                    param_name = KRT_STRDUP(parser->current_token.value);
                    parser_advance(parser);
                    parser_advance(parser);
                    arg = parser_parse_expression(parser);
                    if (PARSER_UNLIKELY(!arg)) {
                        KRT_FREE(param_name);
                        goto cleanup_error;
                    }
                } else {
                    arg = parser_parse_expression(parser);
                    if (PARSER_UNLIKELY(!arg)) {
                        goto cleanup_error;
                    }
                }
            } else {
                arg = parser_parse_expression(parser);
                if (PARSER_UNLIKELY(!arg)) {
                    goto cleanup_error;
                }
            }
            
            if (argument_count >= capacity) {
                capacity *= 2;
                ASTNode** new_arguments = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
                char** new_argument_names = (char**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(char*));
                if (PARSER_UNLIKELY(!new_arguments || !new_argument_names)) {
                    KRT_FREE(param_name);
                    ast_destroy_node(arg);
                    goto cleanup_error;
                }
                memcpy(new_arguments, arguments, argument_count * sizeof(ASTNode*));
                memcpy(new_argument_names, argument_names, argument_count * sizeof(char*));
                arguments = new_arguments;
                argument_names = new_argument_names;
            }
            
            arguments[argument_count] = arg;
            argument_names[argument_count] = param_name;
            argument_count++;
            
            if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                break;
            }
            if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_COMMA)) {
                goto cleanup_error;
            }
            parser_advance(parser);
        }
    }
    
    if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_RIGHT_PAREN)) {
        goto cleanup_error;
    }
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_CALL, parser->current_token.line, parser->current_token.column);
    if (PARSER_UNLIKELY(!node)) {
        goto cleanup_error;
    }
    
    if (callee->type == AST_IDENTIFIER) {
        node->data.call.name = PARSER_STRDUP(callee->data.identifier_name);
        node->data.call.object = NULL;
        ast_destroy_node(callee);
    } else if (callee->type == AST_MEMBER_ACCESS) {
        node->data.call.name = PARSER_STRDUP(callee->data.member_access.member_name);
        ASTNode* object = callee->data.member_access.object;
        callee->data.member_access.object = NULL;
        node->data.call.object = object;
        ast_destroy_node(callee);
    } else {
        node->data.call.name = PARSER_STRDUP("__expr_call__");
        node->data.call.object = callee;
    }
    node->data.call.arguments = arguments;
    node->data.call.argument_count = argument_count;
    node->data.call.argument_names = argument_names;
    return node;
    
cleanup_error:
    for (int i = 0; i < argument_count; i++) {
        ast_destroy_node(arguments[i]);
        if (argument_names && argument_names[i]) {
            KRT_FREE(argument_names[i]);
        }
    }
    ast_destroy_node(callee);
    return NULL;
}
static ASTNode* parser_parse_print_like_statement(Parser* parser, bool has_newline) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode** values = NULL;
    int value_count = 0;
    int capacity = 0;
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        capacity = PARSER_ARGUMENT_CAPACITY_INIT;
        values = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
        if (PARSER_UNLIKELY(!values)) {
            return NULL;
        }
        
        while (1) {
            ASTNode* val = parser_parse_expression(parser);
            if (PARSER_UNLIKELY(!val)) {
                return NULL;
            }
            
            if (value_count >= capacity) {
                capacity *= 2;
                ASTNode** new_values = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
                if (PARSER_UNLIKELY(!new_values)) {
                    return NULL;
                }
                memcpy(new_values, values, value_count * sizeof(ASTNode*));
                values = new_values;
            }
            
            values[value_count++] = val;
            
            if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
            if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_COMMA)) {
                return NULL;
            }
            parser_advance(parser);
        }
    }
    
    if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_RIGHT_PAREN)) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_PRINT_STATEMENT, line, col);
    if (PARSER_UNLIKELY(!node)) {
        return NULL;
    }
    node->data.print_stmt.values = values;
    node->data.print_stmt.value_count = value_count;
    node->data.print_stmt.has_newline = has_newline;
    return node;
}

static ASTNode* parser_parse_console_writeline(Parser* parser) {
    int line = parser->current_token.line;
    (void)line;
    parser_advance(parser); 
    if (parser->current_token.type != TOKEN_DOT) {
        return NULL;
    }
    parser_advance(parser); 
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    bool has_newline = true;
    const char* method = parser->current_token.value;
    if (strcmp(method, "Write") == 0 || strcmp(method, "write") == 0 ||
        strcmp(method, "WriteInt") == 0 || strcmp(method, "writeInt") == 0 || strcmp(method, "writeint") == 0) {
        has_newline = false;
    } else if (strcmp(method, "WriteLine") == 0 || strcmp(method, "writeLine") == 0 || strcmp(method, "writeline") == 0 ||
               strcmp(method, "WriteLineInt") == 0 || strcmp(method, "writeLineInt") == 0 || strcmp(method, "writelineint") == 0) {
        has_newline = true;
    } else {
        return NULL;
    }
    
    parser_advance(parser); 
    return parser_parse_print_like_statement(parser, has_newline);
}

static ASTNode* parser_parse_unsafe_call(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser); 
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser); 
    
    ASTNode* nested_expr = NULL;
    char** permissions = NULL;
    int permission_count = 0;
    int is_block_style = 0;
    
    if (parser->current_token.type == TOKEN_STRING) {
        
        const char* code = parser->current_token.value;
        Lexer* temp_lexer = lexer_create(code);
        Parser* temp_parser = parser_create(temp_lexer);
        temp_parser->is_unsafe_mode = 1;
        nested_expr = parser_parse_expression(temp_parser);
        parser_destroy(temp_parser);
        lexer_destroy(temp_lexer);
        
        parser_advance(parser); 
        
        while (parser->current_token.type == TOKEN_COMMA) {
            parser_advance(parser); 
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                if (nested_expr) ast_destroy_node(nested_expr);
                return NULL;
            }
            permissions = (char**)KRT_REALLOC(permissions, (permission_count + 1) * sizeof(char*));
            permissions[permission_count++] = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
        }
    } else {
        
        is_block_style = 1;
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            while (1) {
                if (parser->current_token.type != TOKEN_IDENTIFIER) {
                    for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
                    KRT_FREE(permissions);
                    return NULL;
                }
                permissions = (char**)KRT_REALLOC(permissions, (permission_count + 1) * sizeof(char*));
                permissions[permission_count++] = KRT_STRDUP(parser->current_token.value);
                parser_advance(parser);
                
                if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
                if (parser->current_token.type != TOKEN_COMMA) {
                    for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
                    KRT_FREE(permissions);
                    return NULL;
                }
                parser_advance(parser);
            }
        }
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        if (nested_expr) ast_destroy_node(nested_expr);
        for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
        KRT_FREE(permissions);
        return NULL;
    }
    parser_advance(parser); 
    
    if (is_block_style) {
        if (parser->current_token.type != TOKEN_LEFT_BRACE) {
            for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
            KRT_FREE(permissions);
            return NULL;
        }
        
        int old_mode = parser->is_unsafe_mode;
        parser->is_unsafe_mode = 1;
        nested_expr = parser_parse_block(parser);
        parser->is_unsafe_mode = old_mode;
    }
    
    if (!nested_expr) {
        for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
        KRT_FREE(permissions);
        return NULL;
    }
    
    ASTNode* node = ast_create_node(AST_UNSAFE_CALL, line, col);
    if (!node) {
        ast_destroy_node(nested_expr);
        for (int i = 0; i < permission_count; i++) KRT_FREE(permissions[i]);
        KRT_FREE(permissions);
        return NULL;
    }
    node->data.unsafe_call.expression = nested_expr;
    node->data.unsafe_call.permissions = permissions;
    node->data.unsafe_call.permission_count = permission_count;
    node->data.unsafe_call.is_block = is_block_style;
    return node;
}
static ASTNode* parser_parse_namespace_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    printf("[DEBUG] parser_parse_namespace_declaration: starting, token=%d\n", parser->current_token.type);
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        printf("[DEBUG] parser_parse_namespace_declaration: expected IDENTIFIER, got %d\n", parser->current_token.type);
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    printf("[DEBUG] parser_parse_namespace_declaration: name=%s\n", name);
    parser_advance(parser);
    printf("[DEBUG] parser_parse_namespace_declaration: after name advance, token=%d\n", parser->current_token.type);
    if (parser->current_token.type == TOKEN_COLON) {
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* body = parser_parse_block(parser);
    printf("[DEBUG] parser_parse_namespace_declaration: body=%p, type=%d\n", (void*)body, body ? (int)body->type : -1);
    if (!body) {
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* node = PARSER_CREATE_NODE(AST_NAMESPACE_DECLARATION, line, col);
    node->data.namespace_decl.name = name;
    node->data.namespace_decl.body = body;
    printf("[DEBUG] parser_parse_namespace_declaration: created node=%p, body=%p\n", (void*)node, (void*)body);
    return node;
}

static ASTNode* parser_parse_using_directive(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser);
    
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        return parser_parse_using_statement(parser);
    }
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    char** parts = NULL;
    int part_count = 0;
    int capacity = 8;
    parts = (char**)KRT_MALLOC(capacity * sizeof(char*));
    
    if (part_count >= capacity) {
        capacity *= 2;
        parts = (char**)KRT_REALLOC(parts, capacity * sizeof(char*));
    }
    parts[part_count++] = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);
    
    while (parser->current_token.type == TOKEN_DOT) {
        parser_advance(parser);
        
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            
            for (int i = 0; i < part_count; i++) {
                KRT_FREE(parts[i]);
            }
            KRT_FREE(parts);
            return NULL;
        }
        
        if (part_count >= capacity) {
            capacity *= 2;
            parts = (char**)KRT_REALLOC(parts, capacity * sizeof(char*));
        }
        parts[part_count++] = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
    }
    
    char* alias = NULL;
    int is_alias = 0;
    
    if (parser->current_token.type == TOKEN_ASSIGN) {
        
        if (part_count != 1) {
            
            for (int i = 0; i < part_count; i++) {
                KRT_FREE(parts[i]);
            }
            KRT_FREE(parts);
            return NULL;
        }
        
        alias = parts[0];
        is_alias = 1;
        
        parser_advance(parser);
        
        parts = NULL;
        part_count = 0;
        
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            if (alias) KRT_FREE(alias);
            return NULL;
        }
        
        if (part_count >= capacity) {
            capacity = 8;
            parts = (char**)KRT_MALLOC(capacity * sizeof(char*));
        }
        parts[part_count++] = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
        
        while (parser->current_token.type == TOKEN_DOT) {
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                
                if (alias) KRT_FREE(alias);
                for (int i = 0; i < part_count; i++) {
                    KRT_FREE(parts[i]);
                }
                KRT_FREE(parts);
                return NULL;
            }
            
            if (part_count >= capacity) {
                capacity *= 2;
                parts = (char**)KRT_REALLOC(parts, capacity * sizeof(char*));
            }
            parts[part_count++] = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
        }
    }
    
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        
        if (alias) KRT_FREE(alias);
        for (int i = 0; i < part_count; i++) {
            KRT_FREE(parts[i]);
        }
        KRT_FREE(parts);
        return NULL;
    }
    
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_USING_DIRECTIVE, line, col);
    node->data.using_directive.alias = alias;
    node->data.using_directive.namespace_path = parts;
    node->data.using_directive.path_length = part_count;
    node->data.using_directive.is_alias = is_alias;
    
    return node;
}

static ASTNode* parser_parse_delegate_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);

    // Parse return type if present (e.g., "void" in "public delegate void Action()")
    // Also handle "function" keyword (e.g., "public delegate function Action()")
    KrtTokenType delegate_return_type = TOKEN_VOID;
    if (parser_is_type_keyword(parser->current_token.type)) {
        delegate_return_type = parser->current_token.type;
        parser_advance(parser);
    } else if (parser->current_token.type == TOKEN_FUNCTION) {
        // "function" keyword is used instead of return type, skip it
        parser_advance(parser);
    }
    (void)delegate_return_type;

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    
    char** parameters = NULL;
    KrtTokenType* parameter_types = NULL;
    int parameter_count = 0;
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        int capacity = 4;
        parameters = (char**)KRT_MALLOC(capacity * sizeof(char*));
        parameter_types = (KrtTokenType*)KRT_MALLOC(capacity * sizeof(KrtTokenType));
        
        if (!parameters || !parameter_types) {
            KRT_FREE(name);
            KRT_FREE(parameters);
            KRT_FREE(parameter_types);
            return NULL;
        }
        
        while (1) {
            KrtTokenType param_type = TOKEN_VOID;
            if (parser_is_type_keyword(parser->current_token.type)) {
                param_type = parser->current_token.type;
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
                param_type = TOKEN_IDENTIFIER;
                parser_advance(parser);
            } else {
                for (int i = 0; i < parameter_count; i++) {
                    KRT_FREE(parameters[i]);
                }
                KRT_FREE(parameters);
                KRT_FREE(parameter_types);
                KRT_FREE(name);
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                for (int i = 0; i < parameter_count; i++) {
                    KRT_FREE(parameters[i]);
                }
                KRT_FREE(parameters);
                KRT_FREE(parameter_types);
                KRT_FREE(name);
                return NULL;
            }
            
            if (parameter_count >= capacity) {
                capacity *= 2;
                char** new_params = (char**)KRT_REALLOC(parameters, capacity * sizeof(char*));
                KrtTokenType* new_types = (KrtTokenType*)KRT_REALLOC(parameter_types, capacity * sizeof(KrtTokenType));
                if (!new_params || !new_types) {
                    for (int i = 0; i < parameter_count; i++) {
                        KRT_FREE(parameters[i]);
                    }
                    KRT_FREE(parameters);
                    KRT_FREE(parameter_types);
                    KRT_FREE(name);
                    return NULL;
                }
                parameters = new_params;
                parameter_types = new_types;
            }
            
            parameters[parameter_count] = KRT_STRDUP(parser->current_token.value);
            parameter_types[parameter_count] = param_type;
            parameter_count++;
            parser_advance(parser);
            
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                break;
            } else {
                for (int i = 0; i < parameter_count; i++) {
                    KRT_FREE(parameters[i]);
                }
                KRT_FREE(parameters);
                KRT_FREE(parameter_types);
                KRT_FREE(name);
                return NULL;
            }
        }
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        for (int i = 0; i < parameter_count; i++) {
            KRT_FREE(parameters[i]);
        }
        KRT_FREE(parameters);
        KRT_FREE(parameter_types);
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    
    KrtTokenType return_type = TOKEN_VOID;
    if (parser->current_token.type == TOKEN_COLON) {
        parser_advance(parser);
        if (parser_is_type_keyword(parser->current_token.type)) {
            return_type = parser->current_token.type;
            parser_advance(parser);
        } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
            return_type = TOKEN_IDENTIFIER;
            parser_advance(parser);
        } else {
            for (int i = 0; i < parameter_count; i++) {
                KRT_FREE(parameters[i]);
            }
            KRT_FREE(parameters);
            KRT_FREE(parameter_types);
            KRT_FREE(name);
            return NULL;
        }
    }
    
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    
    ASTNode* node = ast_create_node(AST_DELEGATE_DECLARATION, line, col);
    if (!node) {
        for (int i = 0; i < parameter_count; i++) {
            KRT_FREE(parameters[i]);
        }
        KRT_FREE(parameters);
        KRT_FREE(parameter_types);
        KRT_FREE(name);
        return NULL;
    }
    
    node->data.delegate_decl.name = name;
    node->data.delegate_decl.parameters = parameters;
    node->data.delegate_decl.parameter_types = parameter_types;
    node->data.delegate_decl.parameter_count = parameter_count;
    node->data.delegate_decl.return_type = return_type;
    
    return node;
}

static ASTNode* parser_parse_class_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    printf("[DEBUG] parser_parse_class_declaration: starting, token=%d\n", parser->current_token.type);
    parser_advance(parser);
    printf("[DEBUG] parser_parse_class_declaration: after advance, token=%d, value=%s\n", parser->current_token.type, parser->current_token.value ? parser->current_token.value : "null");
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        printf("[DEBUG] parser_parse_class_declaration: expected IDENTIFIER, got %d\n", parser->current_token.type);
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);

    char* old_class = parser->current_class;
    parser->current_class = name;

    // Parse generic type parameters if present
    ASTNode** generic_params = NULL;
    int generic_param_count = 0;
    if (parser->current_token.type == TOKEN_LESS) {
        parser_advance(parser); // consume '<'

        // Parse type parameters
        int capacity = 4;
        generic_params = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
        if (!generic_params) {
            KRT_FREE(name);
            parser->current_class = old_class;
            return NULL;
        }

        while (parser->current_token.type == TOKEN_IDENTIFIER) {
            if (generic_param_count >= capacity) {
                capacity *= 2;
                ASTNode** new_params = (ASTNode**)KRT_REALLOC(generic_params, capacity * sizeof(ASTNode*));
                if (!new_params) {
                    for (int i = 0; i < generic_param_count; i++) {
                        ast_destroy_node(generic_params[i]);
                    }
                    KRT_FREE(generic_params);
                    KRT_FREE(name);
                    parser->current_class = old_class;
                    return NULL;
                }
                generic_params = new_params;
            }

            ASTNode* param = PARSER_CREATE_NODE(AST_TEMPLATE_PARAMETER, line, col);
            if (!param) {
                for (int i = 0; i < generic_param_count; i++) {
                    ast_destroy_node(generic_params[i]);
                }
                KRT_FREE(generic_params);
                KRT_FREE(name);
                parser->current_class = old_class;
                return NULL;
            }
            param->data.template_param.param_name = KRT_STRDUP(parser->current_token.value);
            generic_params[generic_param_count++] = param;

            parser_advance(parser);

            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else {
                break;
            }
        }

        if (parser->current_token.type != TOKEN_GREATER) {
            for (int i = 0; i < generic_param_count; i++) {
                ast_destroy_node(generic_params[i]);
            }
            KRT_FREE(generic_params);
            KRT_FREE(name);
            parser->current_class = old_class;
            return NULL;
        }
        parser_advance(parser); // consume '>'
    }

    // Parse generic constraints if present
    ASTNode* constraints = NULL;
    if (parser->current_token.type == TOKEN_WHERE) {
        constraints = parser_parse_generic_constraints(parser);
    }

    ASTNode* base_class = NULL;
    if (parser->current_token.type == TOKEN_COLON) {
        int bc_line = parser->current_token.line;
        int bc_col = parser->current_token.column;
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            KRT_FREE(name);
            parser->current_class = old_class;
            return NULL;
        }
        base_class = ast_create_node(AST_IDENTIFIER, bc_line, bc_col);
        base_class->data.identifier_name = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
    }
    ASTNode* body = parser_parse_block(parser);

    parser->current_class = old_class;

    if (!body) {
        printf("[DEBUG] parser_parse_class_declaration: body is NULL\n");
        KRT_FREE(name);
        if (base_class) {
            ast_destroy_node(base_class);
        }
        for (int i = 0; i < generic_param_count; i++) {
            ast_destroy_node(generic_params[i]);
        }
        KRT_FREE(generic_params);
        return NULL;
    }
    printf("[DEBUG] parser_parse_class_declaration: creating class node for %s\n", name);
    ASTNode* node = PARSER_CREATE_NODE(AST_CLASS_DECLARATION, line, col);
    node->data.class_decl.name = name;
    node->data.class_decl.body = body;
    node->data.class_decl.base_class = base_class;
    node->data.class_decl.template_params = (char**)generic_params;
    node->data.class_decl.template_param_count = generic_param_count;
    node->data.class_decl.constraints = (ASTNode**)constraints;
    node->data.class_decl.constraint_count = constraints ? 1 : 0;
    return node;
}

static KrtTokenType parser_peek_after_closing_paren(Parser* parser) {
    if (!parser || !parser->lexer) return TOKEN_EOF;
    Lexer saved = *parser->lexer;
    int depth = 0;
    KrtTokenType result = TOKEN_EOF;
    while (1) {
        Token t = lexer_next_token(parser->lexer);
        if (t.type == TOKEN_LEFT_PAREN) {
            depth++;
        } else if (t.type == TOKEN_RIGHT_PAREN) {
            if (depth == 0) {
                Token after = lexer_next_token(parser->lexer);
                result = after.type;
                break;
            }
            depth--;
        } else if (t.type == TOKEN_EOF) {
            break;
        }
    }
    *parser->lexer = saved;
    return result;
}

static ASTNode* parser_parse_primary(Parser* parser) {
    ASTNode* node = NULL;
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    if (parser_is_type_keyword(parser->current_token.type)) {
        Token peek = lexer_peek_token(parser->lexer);
        if (peek.type == TOKEN_LEFT_PAREN) {
            // This is an explicit cast: Type(expression)
            KrtTokenType target_type = parser->current_token.type;
            token_free(&peek);
            parser_advance(parser); // consume type
            parser_advance(parser); // consume '('
            
            ASTNode* expr = parser_parse_expression(parser);
            if (!expr) {
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                ast_destroy_node(expr);
                return NULL;
            }
            parser_advance(parser); // consume ')'
            
            node = ast_create_node(AST_CAST_EXPRESSION, line, col);
            if (!node) {
                ast_destroy_node(expr);
                return NULL;
            }
            node->data.cast_expr.target_type = target_type;
            node->data.cast_expr.expression = expr;
            return node;
        }
        token_free(&peek);
    }

    switch (parser->current_token.type) {
        case TOKEN_NUMBER:
            node = ast_create_node(AST_NUMBER, line, col);
            node->data.number_value = atof(parser->current_token.value);
            parser_advance(parser);
            break;
        case TOKEN_STRING:
            node = ast_create_node(AST_STRING, line, col);
            node->data.string_value = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
            break;
        case TOKEN_DOLLAR: {
            // Interpolated string: $"Hello {name}!"
            parser_advance(parser); // consume $
            
            if (parser->current_token.type != TOKEN_STRING) {
                return NULL;
            }
            
            // Parse the string literal and extract interpolated parts
            char* raw_string = parser->current_token.value;
            parser_advance(parser); // consume string
            
            // Simple implementation: parse the string for {expression} patterns
            node = ast_create_node(AST_INTERPOLATED_STRING, line, col);
            if (!node) {
                return NULL;
            }
            
            // Allocate arrays for parts and expressions
            int capacity = 8;
            char** parts = (char**)KRT_MALLOC(capacity * sizeof(char*));
            ASTNode** expressions = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
            if (!parts || !expressions) {
                if (parts) KRT_FREE(parts);
                if (expressions) KRT_FREE(expressions);
                ast_destroy_node(node);
                return NULL;
            }
            
            int part_count = 0;
            int expr_count = 0;
            
            // Parse the raw string for interpolation
            // Format: "text {expr} more text {expr2} end"
            char* str = KRT_STRDUP(raw_string);
            if (!str) {
                KRT_FREE(parts);
                KRT_FREE(expressions);
                ast_destroy_node(node);
                return NULL;
            }
            
            char* p = str;
            char* current_part = (char*)KRT_MALLOC(strlen(str) + 1);
            if (!current_part) {
                KRT_FREE(str);
                KRT_FREE(parts);
                KRT_FREE(expressions);
                ast_destroy_node(node);
                return NULL;
            }
            int part_idx = 0;
            
            while (*p) {
                if (*p == '{' && *(p+1) != '{') {
                    // End current part
                    current_part[part_idx] = '\0';
                    if (part_idx > 0 || part_count == 0) {
                        parts[part_count++] = KRT_STRDUP(current_part);
                    }
                    part_idx = 0;
                    p++;
                    
                    // Parse expression until }
                    char expr_buf[256];
                    int expr_idx = 0;
                    int brace_depth = 1;
                    while (*p && brace_depth > 0) {
                        if (*p == '{') brace_depth++;
                        else if (*p == '}') brace_depth--;
                        
                        if (brace_depth > 0) {
                            expr_buf[expr_idx++] = *p;
                        }
                        p++;
                    }
                    expr_buf[expr_idx] = '\0';
                    
                    // Create a simple identifier node for the expression
                    // In a full implementation, we'd parse the expression properly
                    ASTNode* expr_node = ast_create_node(AST_IDENTIFIER, line, col);
                    if (expr_node) {
                        expr_node->data.identifier_name = KRT_STRDUP(expr_buf);
                        expressions[expr_count++] = expr_node;
                    }
                } else {
                    // Skip escaped braces
                    if (*p == '{' && *(p+1) == '{') p++;
                    else if (*p == '}' && *(p+1) == '}') p++;
                    
                    current_part[part_idx++] = *p;
                    p++;
                }
            }
            
            // Add final part
            current_part[part_idx] = '\0';
            if (part_idx > 0) {
                parts[part_count++] = KRT_STRDUP(current_part);
            }
            
            KRT_FREE(current_part);
            KRT_FREE(str);
            
            node->data.interpolated_string.string_parts = parts;
            node->data.interpolated_string.part_count = part_count;
            node->data.interpolated_string.expressions = expressions;
            node->data.interpolated_string.expression_count = expr_count;
            
            return node;
        }
        case TOKEN_TRUE:
            node = ast_create_node(AST_BOOLEAN, line, col);
            node->data.boolean_value = 1;
            parser_advance(parser);
            break;
        case TOKEN_FALSE:
            node = ast_create_node(AST_BOOLEAN, line, col);
            node->data.boolean_value = 0;
            parser_advance(parser);
            break;
        case TOKEN_NULL:
            node = ast_create_node(AST_NULL, line, col);
            parser_advance(parser);
            break;
        case TOKEN_SIZEOF: {
            parser_advance(parser); // consume sizeof
            
            if (parser->current_token.type != TOKEN_LEFT_PAREN) {
                return NULL;
            }
            parser_advance(parser); // consume '('
            
            KrtTokenType type_token = TOKEN_EOF;
            char* type_name = NULL;
            
            if (parser_is_type_keyword(parser->current_token.type)) {
                type_token = parser->current_token.type;
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
                type_name = KRT_STRDUP(parser->current_token.value);
                if (!type_name) {
                    return NULL;
                }
                parser_advance(parser);
            } else {
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                if (type_name) KRT_FREE(type_name);
                return NULL;
            }
            parser_advance(parser); // consume ')'
            
            node = ast_create_node(AST_SIZEOF_EXPRESSION, line, col);
            if (!node) {
                if (type_name) KRT_FREE(type_name);
                return NULL;
            }
            node->data.sizeof_expr.type_token = type_token;
            node->data.sizeof_expr.type_name = type_name;
            return node;
        }
        case TOKEN_IDENTIFIER:
            if (!parser->is_unsafe_mode && parser->current_token.value[0] == '_' &&
                strncmp(parser->current_token.value, "__lambda_", 9) != 0) {
                return NULL;
            }
            node = ast_create_node(AST_IDENTIFIER, line, col);
            if (!node) {
                return NULL;
            }
            node->data.identifier_name = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
            break;

        case TOKEN_LEFT_PAREN: {

            Token peek1 = lexer_peek_token(parser->lexer);
            if (peek1.type == TOKEN_IDENTIFIER) {
                token_free(&peek1);
                if (parser_peek_after_closing_paren(parser) == TOKEN_LAMBDA) {
                    parser_advance(parser); // consume '('
                    char** params = NULL;
                    int param_count = 0;
                    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                        parser_advance(parser);
                    } else {
                        int capacity = 4;
                        params = (char**)KRT_MALLOC(capacity * sizeof(char*));
                        if (!params) return NULL;
                        while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                                for (int i = 0; i < param_count; i++) KRT_FREE(params[i]);
                                KRT_FREE(params);
                                return NULL;
                            }
                            if (param_count >= capacity) {
                                capacity *= 2;
                                char** new_params = (char**)KRT_REALLOC(params, capacity * sizeof(char*));
                                if (!new_params) {
                                    for (int i = 0; i < param_count; i++) KRT_FREE(params[i]);
                                    KRT_FREE(params);
                                    return NULL;
                                }
                                params = new_params;
                            }
                            params[param_count] = KRT_STRDUP(parser->current_token.value);
                            param_count++;
                            parser_advance(parser);
                            if (parser->current_token.type == TOKEN_COMMA) {
                                parser_advance(parser);
                            } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                                for (int i = 0; i < param_count; i++) KRT_FREE(params[i]);
                                KRT_FREE(params);
                                return NULL;
                            }
                        }
                        parser_advance(parser); // consume ')'
                    }
                    if (parser->current_token.type == TOKEN_LAMBDA) {
                        return parser_parse_lambda_expression(parser, params, param_count);
                    }
                    for (int i = 0; i < param_count; i++) KRT_FREE(params[i]);
                    KRT_FREE(params);
                    return NULL;
                }
                // Not a lambda: fall through to normal parenthesized expression handling
            }
            token_free(&peek1);
            
            parser_advance(parser);
            
            // Check for tuple type: (int, string)
            if (parser_is_type_keyword(parser->current_token.type)) {
                Token peek2 = lexer_peek_token(parser->lexer);
                if (peek2.type == TOKEN_COMMA) {
                    // This is a tuple type
                    token_free(&peek2);
                    
                    int capacity = 4;
                    KrtTokenType* types = (KrtTokenType*)KRT_MALLOC(capacity * sizeof(KrtTokenType));
                    if (!types) {
                        return NULL;
                    }
                    int type_count = 0;
                    
                    while (parser_is_type_keyword(parser->current_token.type)) {
                        if (type_count >= capacity) {
                            capacity *= 2;
                            KrtTokenType* new_types = (KrtTokenType*)KRT_REALLOC(types, capacity * sizeof(KrtTokenType));
                            if (!new_types) {
                                KRT_FREE(types);
                                return NULL;
                            }
                            types = new_types;
                        }
                        types[type_count++] = parser->current_token.type;
                        parser_advance(parser);
                        
                        if (parser->current_token.type == TOKEN_COMMA) {
                            parser_advance(parser);
                        } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                            KRT_FREE(types);
                            return NULL;
                        }
                    }
                    
                    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                        KRT_FREE(types);
                        return NULL;
                    }
                    parser_advance(parser);
                    
                    node = ast_create_node(AST_TUPLE_TYPE, line, col);
                    if (!node) {
                        KRT_FREE(types);
                        return NULL;
                    }
                    node->data.tuple_type.element_types = types;
                    node->data.tuple_type.element_count = type_count;
                    return node;
                }
                token_free(&peek2);
                
                // Regular cast: (type)expression
                KrtTokenType cast_type = parser->current_token.type;
                parser_advance(parser);
                if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                    return NULL;
                }
                parser_advance(parser);
                ASTNode* operand = parser_parse_primary(parser);
                if (!operand) {
                    return NULL;
                }
                node = ast_create_node(AST_UNARY_OPERATION, line, col);
                node->data.unary_op.operator = cast_type;
                node->data.unary_op.operand = operand;
                node->data.unary_op.is_postfix = false;
                break;
            }
            
            // Check for tuple expression: (1, "hello")
            if (parser->current_token.type == TOKEN_NUMBER || 
                parser->current_token.type == TOKEN_STRING ||
                parser->current_token.type == TOKEN_IDENTIFIER ||
                parser->current_token.type == TOKEN_TRUE ||
                parser->current_token.type == TOKEN_FALSE) {
                
                // Parse first element
                ASTNode* first = parser_parse_expression(parser);
                if (!first) {
                    return NULL;
                }
                
                if (parser->current_token.type == TOKEN_COMMA) {
                    // This is a tuple expression
                    int capacity = 4;
                    ASTNode** elements = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
                    if (!elements) {
                        ast_destroy_node(first);
                        return NULL;
                    }
                    elements[0] = first;
                    int element_count = 1;
                    
                    while (parser->current_token.type == TOKEN_COMMA) {
                        parser_advance(parser);
                        
                        ASTNode* elem = parser_parse_expression(parser);
                        if (!elem) {
                            for (int i = 0; i < element_count; i++) {
                                ast_destroy_node(elements[i]);
                            }
                            KRT_FREE(elements);
                            return NULL;
                        }
                        
                        if (element_count >= capacity) {
                            capacity *= 2;
                            ASTNode** new_elements = (ASTNode**)KRT_REALLOC(elements, capacity * sizeof(ASTNode*));
                            if (!new_elements) {
                                for (int i = 0; i < element_count; i++) {
                                    ast_destroy_node(elements[i]);
                                }
                                KRT_FREE(elements);
                                return NULL;
                            }
                            elements = new_elements;
                        }
                        elements[element_count++] = elem;
                    }
                    
                    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy_node(elements[i]);
                        }
                        KRT_FREE(elements);
                        return NULL;
                    }
                    parser_advance(parser);
                    
                    node = ast_create_node(AST_TUPLE_EXPRESSION, line, col);
                    if (!node) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy_node(elements[i]);
                        }
                        KRT_FREE(elements);
                        return NULL;
                    }
                    node->data.tuple_expr.elements = elements;
                    node->data.tuple_expr.element_count = element_count;
                    return node;
                } else {
                    // Just a parenthesized expression
                    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                        ast_destroy_node(first);
                        return NULL;
                    }
                    parser_advance(parser);
                    return first;
                }
            }
            
            node = parser_parse_expression(parser);
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                ast_destroy_node(node);
                return NULL;
            }
            parser_advance(parser);
            break;
        }
        case TOKEN_LEFT_BRACKET:
            parser_advance(parser);
            ASTNode** elements = NULL;
            int element_count = 0;
            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                while (1) {
                    ASTNode* element = parser_parse_expression(parser);
                    if (!element) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy_node(elements[i]);
                        }
                        KRT_FREE(elements);
                        return NULL;
                    }
                    ASTNode** new_elements = (ASTNode**)KRT_REALLOC(elements, (element_count + 1) * sizeof(ASTNode*));
                    if (!new_elements) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy_node(elements[i]);
                        }
                        KRT_FREE(elements);
                        return NULL;
                    }
                    elements = new_elements;
                    elements[element_count] = element;
                    element_count++;
                    if (parser->current_token.type == TOKEN_RIGHT_BRACKET) {
                        break;
                    }
                    if (parser->current_token.type != TOKEN_COMMA) {
                        for (int i = 0; i < element_count; i++) {
                            ast_destroy_node(elements[i]);
                        }
                        KRT_FREE(elements);
                        return NULL;
                    }
                    parser_advance(parser);
                }
            }
            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                for (int i = 0; i < element_count; i++) {
                    ast_destroy_node(elements[i]);
                }
                KRT_FREE(elements);
                return NULL;
            }
            parser_advance(parser);
            node = ast_create_node(AST_ARRAY_LITERAL, line, col);
            node->data.array_literal.elements = elements;
            node->data.array_literal.element_count = element_count;
            break;
        case TOKEN_NEW: return parser_parse_new_expression(parser);
        case TOKEN_STACKALLOC: return parser_parse_stackalloc_expression(parser);
        case TOKEN_THIS:
            parser_advance(parser);
            return ast_create_node(AST_THIS, line, col);
        case TOKEN_MINUS:
        case TOKEN_PLUS: {
            KrtTokenType op = parser->current_token.type;
            parser_advance(parser);
            ASTNode* operand = parser_parse_primary(parser);
            if (!operand) {
                return NULL;
            }
            ASTNode* node = ast_create_node(AST_UNARY_OPERATION, line, col);
            if (!node) {
                ast_destroy_node(operand);
                return NULL;
            }
            node->data.unary_op.operator = op;
            node->data.unary_op.operand = operand;
            node->data.unary_op.is_postfix = false;
            return node;
        }
        case TOKEN_OP_MULTIPLY: {
            // Pointer dereference: *ptr
            parser_advance(parser);
            ASTNode* pointer = parser_parse_primary(parser);
            if (!pointer) {
                return NULL;
            }
            ASTNode* node = ast_create_node(AST_POINTER_DEREFERENCE, line, col);
            if (!node) {
                ast_destroy_node(pointer);
                return NULL;
            }
            node->data.pointer_deref.pointer = pointer;
            return node;
        }
        case TOKEN_AND: {
            // Address-of: &var
            parser_advance(parser);
            ASTNode* operand = parser_parse_primary(parser);
            if (!operand) {
                return NULL;
            }
            ASTNode* node = ast_create_node(AST_ADDRESS_OF, line, col);
            if (!node) {
                ast_destroy_node(operand);
                return NULL;
            }
            node->data.address_of.operand = operand;
            return node;
        }
        case TOKEN_TEMPLATE: {
            int line = parser->current_token.line;
            int col = parser->current_token.column;
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_LESS) {
                return NULL;
            }
            parser_advance(parser);
            
            char** type_args = NULL;
            int type_arg_count = 0;
            
            if (parser->current_token.type != TOKEN_GREATER) {
                while (1) {
                    if (parser->current_token.type != TOKEN_IDENTIFIER) {
                        for (int i = 0; i < type_arg_count; i++) {
                            if (type_args[i]) KRT_FREE(type_args[i]);
                        }
                        KRT_FREE(type_args);
                        return NULL;
                    }
                    
                    char* type_arg = KRT_STRDUP(parser->current_token.value);
                    if (!type_arg) {
                        for (int i = 0; i < type_arg_count; i++) {
                            if (type_args[i]) KRT_FREE(type_args[i]);
                        }
                        KRT_FREE(type_args);
                        return NULL;
                    }
                    
                    type_args = (char**)KRT_REALLOC(type_args, (type_arg_count + 1) * sizeof(char*));
                    type_args[type_arg_count++] = type_arg;
                    parser_advance(parser);
                    
                    if (parser->current_token.type == TOKEN_GREATER) {
                        break;
                    }
                    if (parser->current_token.type != TOKEN_COMMA) {
                        for (int i = 0; i < type_arg_count; i++) {
                            if (type_args[i]) KRT_FREE(type_args[i]);
                        }
                        KRT_FREE(type_args);
                        return NULL;
                    }
                    parser_advance(parser);
                }
            }
            
            if (parser->current_token.type != TOKEN_GREATER) {
                for (int i = 0; i < type_arg_count; i++) {
                    if (type_args[i]) KRT_FREE(type_args[i]);
                }
                KRT_FREE(type_args);
                return NULL;
            }
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_LEFT_PAREN) {
                for (int i = 0; i < type_arg_count; i++) {
                    if (type_args[i]) KRT_FREE(type_args[i]);
                }
                KRT_FREE(type_args);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode** args = NULL;
            int arg_count = 0;
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                while (1) {
                    ASTNode* arg = parser_parse_expression(parser);
                    if (!arg) {
                        for (int i = 0; i < type_arg_count; i++) {
                            if (type_args[i]) KRT_FREE(type_args[i]);
                        }
                        KRT_FREE(type_args);
                        for (int i = 0; i < arg_count; i++) {
                            ast_destroy_node(args[i]);
                        }
                        KRT_FREE(args);
                        return NULL;
                    }
                    
                    args = (ASTNode**)KRT_REALLOC(args, (arg_count + 1) * sizeof(ASTNode*));
                    args[arg_count++] = arg;
                    
                    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                        break;
                    }
                    if (parser->current_token.type != TOKEN_COMMA) {
                        for (int i = 0; i < type_arg_count; i++) {
                            if (type_args[i]) KRT_FREE(type_args[i]);
                        }
                        KRT_FREE(type_args);
                        for (int i = 0; i < arg_count; i++) {
                            ast_destroy_node(args[i]);
                        }
                        KRT_FREE(args);
                        return NULL;
                    }
                    parser_advance(parser);
                }
            }
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                for (int i = 0; i < type_arg_count; i++) {
                    if (type_args[i]) KRT_FREE(type_args[i]);
                }
                KRT_FREE(type_args);
                for (int i = 0; i < arg_count; i++) {
                    ast_destroy_node(args[i]);
                }
                KRT_FREE(args);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* node = ast_create_node(AST_TEMPLATE_INSTANTIATION, line, col);
            if (!node) {
                for (int i = 0; i < type_arg_count; i++) {
                    if (type_args[i]) KRT_FREE(type_args[i]);
                }
                KRT_FREE(type_args);
                for (int i = 0; i < arg_count; i++) {
                    ast_destroy_node(args[i]);
                }
                KRT_FREE(args);
                return NULL;
            }
            
            node->data.template_instantiation.name = NULL;
            node->data.template_instantiation.type_args = type_args;
            node->data.template_instantiation.type_arg_count = type_arg_count;
            node->data.template_instantiation.args = args;
            node->data.template_instantiation.arg_count = arg_count;
            
            return node;
        }
        
        case TOKEN_FROM:
            return parser_parse_linq_query(parser);
        case TOKEN_DEFAULT: {
            int line = parser->current_token.line;
            int col = parser->current_token.column;
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_LEFT_PAREN) {
                return NULL;
            }
            parser_advance(parser);
            
            char* type_name = NULL;
            ASTNode* type_expr = NULL;
            
            if (parser->current_token.type == TOKEN_IDENTIFIER) {
                type_name = KRT_STRDUP(parser->current_token.value);
                parser_advance(parser);
                
                if (parser->current_token.type == TOKEN_LESS) {
                    type_expr = parser_parse_expression(parser);
                }
            } else if (parser_is_type_keyword(parser->current_token.type)) {
                const char* type_str = token_type_to_string(parser->current_token.type);
                type_name = KRT_STRDUP(type_str ? type_str : "UNKNOWN");
                parser_advance(parser);
            } else {
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                if (type_name) KRT_FREE(type_name);
                if (type_expr) ast_destroy_node(type_expr);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* node = ast_create_node(AST_DEFAULT_EXPRESSION, line, col);
            if (!node) {
                if (type_name) KRT_FREE(type_name);
                if (type_expr) ast_destroy_node(type_expr);
                return NULL;
            }
            node->data.default_expr.type_name = type_name;
            node->data.default_expr.type_expr = type_expr;
            return node;
        }
        default:
            break;
    }
    return node;
}

static int get_operator_precedence(KrtTokenType type) {
    switch (type) {
        case TOKEN_OR: return 1;
        case TOKEN_AND: return 2;
        case TOKEN_BITWISE_OR: return 3;
        case TOKEN_BITWISE_XOR: return 4;
        case TOKEN_BITWISE_AND: return 5;
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL: return 6;
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL: return 7;
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT: return 8;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 9;
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO:
            return 10;
        case TOKEN_POWER:
            return 11;
        default:
            return 0;
    }
}

static ASTNode* parser_parse_binary_operation(Parser* parser, int precedence) {
    ASTNode* left = parser_parse_postfix_expression(parser);
    if (PARSER_UNLIKELY(!left)) {
        return NULL;
    }
    while (1) {
        KrtTokenType operator = parser->current_token.type;
        int op_precedence = get_operator_precedence(operator);
        if (op_precedence <= precedence) {
            break;
        }
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        parser_advance(parser);
        ASTNode* right = parser_parse_binary_operation(parser, op_precedence);
        if (PARSER_UNLIKELY(!right)) {
            ast_destroy_node(left);
            return NULL;
        }
        ASTNode* new_node = ast_create_node(AST_BINARY_OPERATION, line, col);
        if (PARSER_UNLIKELY(!new_node)) {
            ast_destroy_node(left);
            ast_destroy_node(right);
            return NULL;
        }
        new_node->data.binary_op.left = left;
        new_node->data.binary_op.operator = operator;
        new_node->data.binary_op.right = right;
        left = new_node;
    }
    return left;
}

static ASTNode* parser_parse_postfix_expression(Parser* parser) {
    ASTNode* left = parser_parse_primary_debug(parser);
    if (!left) {
        return NULL;
    }
    while (parser->current_token.type == TOKEN_DOT ||
           parser->current_token.type == TOKEN_QUESTION_DOT ||
           parser->current_token.type == TOKEN_DOUBLE_COLON ||
           parser->current_token.type == TOKEN_LEFT_BRACKET) {
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
            parser_advance(parser);
            ASTNode* index = parser_parse_expression(parser);
            if (!index) {
                ast_destroy_node(left);
                return NULL;
            }
            if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
                ast_destroy_node(left);
                ast_destroy_node(index);
                return NULL;
            }
            parser_advance(parser);
            ASTNode* array_access = ast_create_node(AST_ARRAY_ACCESS, line, col);
            if (array_access) {
                array_access->data.array_access.array = left;
                array_access->data.array_access.index = index;
                left = array_access;
            } else {
                ast_destroy_node(left);
                ast_destroy_node(index);
                return NULL;
            }
        } else if (parser->current_token.type == TOKEN_DOT ||
                   parser->current_token.type == TOKEN_QUESTION_DOT) {
            int is_null_conditional = (parser->current_token.type == TOKEN_QUESTION_DOT);
            parser_advance(parser);
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                ast_destroy_node(left);
                return NULL;
            }
            char* member_name = KRT_STRDUP(parser->current_token.value);
            if (!member_name) {
                ast_destroy_node(left);
                return NULL;
            }
            parser_advance(parser);
            
            if (is_null_conditional) {
                ASTNode* node = ast_create_node(AST_NULL_CONDITIONAL, line, col);
                if (!node) {
                    KRT_FREE(member_name);
                    ast_destroy_node(left);
                    return NULL;
                }
                node->data.null_conditional.expression = left;
                node->data.null_conditional.member_name = member_name;
                node->data.null_conditional.is_method_call = 0;
                
                if (parser->current_token.type == TOKEN_LEFT_PAREN) {
                    node->data.null_conditional.is_method_call = 1;
                    ASTNode* call_node = parser_parse_call(parser, left);
                    if (!call_node) {
                        return NULL;
                    }
                    node->data.null_conditional.arguments = call_node;
                }
                left = node;
            } else {
                ASTNode* node = ast_create_node(AST_MEMBER_ACCESS, line, col);
                if (!node) {
                    KRT_FREE(member_name);
                    ast_destroy_node(left);
                    return NULL;
                }
                node->data.member_access.object = left;
                node->data.member_access.member_name = member_name;
                left = node;
                if (parser->current_token.type == TOKEN_LEFT_PAREN) {
                    ASTNode* call_node = parser_parse_call(parser, left);
                    if (!call_node) {
                        return NULL;
                    }
                    left = call_node;
                }
            }
        } else if (parser->current_token.type == TOKEN_DOUBLE_COLON) {
            parser_advance(parser);
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                ast_destroy_node(left);
                return NULL;
            }
            char* method_name = KRT_STRDUP(parser->current_token.value);
            if (!method_name) {
                ast_destroy_node(left);
                return NULL;
            }
            parser_advance(parser);
            if (left->type != AST_IDENTIFIER) {
                KRT_FREE(method_name);
                ast_destroy_node(left);
                return NULL;
            }
            char* class_name = KRT_STRDUP(left->data.identifier_name);
            if (!class_name) {
                KRT_FREE(method_name);
                ast_destroy_node(left);
                return NULL;
            }
            if (parser->current_token.type != TOKEN_LEFT_PAREN) {
                KRT_FREE(class_name);
                KRT_FREE(method_name);
                ast_destroy_node(left);
                return NULL;
            }
            parser_advance(parser);
            ASTNode** arguments = NULL;
            int argument_count = 0;
            int capacity = 0;
            
            if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                capacity = PARSER_ARGUMENT_CAPACITY_INIT;
                arguments = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
                if (PARSER_UNLIKELY(!arguments)) {
                    KRT_FREE(class_name);
                    KRT_FREE(method_name);
                    ast_destroy_node(left);
                    return NULL;
                }
                
                while (1) {
                    ASTNode* arg = parser_parse_expression(parser);
                    if (PARSER_UNLIKELY(!arg)) {
                        goto static_call_cleanup;
                    }
                    
                    if (argument_count >= capacity) {
                        capacity *= 2;
                        ASTNode** new_args = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, capacity * sizeof(ASTNode*));
                        if (PARSER_UNLIKELY(!new_args)) {
                            ast_destroy_node(arg);
                            goto static_call_cleanup;
                        }
                        memcpy(new_args, arguments, argument_count * sizeof(ASTNode*));
                        arguments = new_args;
                    }
                    
                    arguments[argument_count] = arg;
                    argument_count++;
                    
                    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                        break;
                    }
                    if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_COMMA)) {
                        goto static_call_cleanup;
                    }
                    parser_advance(parser);
                }
            }
            
            if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_RIGHT_PAREN)) {
                goto static_call_cleanup;
            }
            parser_advance(parser);
            
            ASTNode* static_call_node = ast_create_node(AST_STATIC_METHOD_CALL, line, col);
            if (PARSER_UNLIKELY(!static_call_node)) {
                goto static_call_cleanup;
            }
            static_call_node->data.static_call.class_name = class_name;
            static_call_node->data.static_call.method_name = method_name;
            static_call_node->data.static_call.arguments = arguments;
            static_call_node->data.static_call.argument_count = argument_count;
            ast_destroy_node(left);
            left = static_call_node;
            continue;
            
        static_call_cleanup:
            for (int i = 0; i < argument_count; i++) {
                ast_destroy_node(arguments[i]);
            }
            KRT_FREE(class_name);
            KRT_FREE(method_name);
            ast_destroy_node(left);
            return NULL;
        }
    }
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        ASTNode* call_node = parser_parse_call(parser, left);
        if (!call_node) {
            return NULL;
        }
        left = call_node;
    }
    if (parser->current_token.type == TOKEN_INCREMENT) {
        int inc_line = parser->current_token.line;
        int inc_col = parser->current_token.column;
        parser_advance(parser);
        ASTNode* node = ast_create_node(AST_UNARY_OPERATION, inc_line, inc_col);
        node->data.unary_op.operator = TOKEN_INCREMENT;
        node->data.unary_op.operand = left;
        node->data.unary_op.is_postfix = true;
        return node;
    }
    
    if (parser->current_token.type == TOKEN_IS || parser->current_token.type == TOKEN_AS) {
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        KrtTokenType op_type = parser->current_token.type;
        parser_advance(parser);
        
        char* type_name = NULL;
        ASTNode* type_expr = NULL;
        
        if (parser->current_token.type == TOKEN_IDENTIFIER) {
            type_name = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
            
            if (parser->current_token.type == TOKEN_LESS) {
                type_expr = parser_parse_expression(parser);
            }
        } else if (parser_is_type_keyword(parser->current_token.type)) {
            const char* type_str = token_type_to_string(parser->current_token.type);
            type_name = KRT_STRDUP(type_str ? type_str : "UNKNOWN");
            parser_advance(parser);
        } else {
            return NULL;
        }
        
        ASTNode* node = ast_create_node(
            op_type == TOKEN_IS ? AST_IS_EXPRESSION : AST_AS_EXPRESSION,
            line, col
        );
        if (!node) {
            if (type_name) KRT_FREE(type_name);
            if (type_expr) ast_destroy_node(type_expr);
            ast_destroy_node(left);
            return NULL;
        }
        
        if (op_type == TOKEN_IS) {
            node->data.is_expr.expression = left;
            node->data.is_expr.type_name = type_name;
            node->data.is_expr.type_expr = type_expr;
        } else {
            node->data.as_expr.expression = left;
            node->data.as_expr.type_name = type_name;
            node->data.as_expr.type_expr = type_expr;
        }
        return node;
    }
    
    return left;
}

static ASTNode* parser_parse_ternary_operation(Parser* parser) {
    ASTNode* condition = parser_parse_binary_operation(parser, 0);
    if (PARSER_UNLIKELY(!condition)) {
        return NULL;
    }
    if (parser->current_token.type == TOKEN_QUESTION) {
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        parser_advance(parser);
        ASTNode* true_value = parser_parse_binary_operation(parser, 0);
        if (PARSER_UNLIKELY(!true_value)) {
            ast_destroy_node(condition);
            return NULL;
        }
        if (PARSER_UNLIKELY(parser->current_token.type != TOKEN_COLON)) {
            ast_destroy_node(condition);
            ast_destroy_node(true_value);
            return NULL;
        }
        parser_advance(parser);
        ASTNode* false_value = parser_parse_binary_operation(parser, 0);
        if (PARSER_UNLIKELY(!false_value)) {
            ast_destroy_node(condition);
            ast_destroy_node(true_value);
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_TERNARY_OPERATION, line, col);
        if (PARSER_UNLIKELY(!node)) {
            ast_destroy_node(condition);
            ast_destroy_node(true_value);
            ast_destroy_node(false_value);
            return NULL;
        }
        node->data.ternary_op.condition = condition;
        node->data.ternary_op.true_value = true_value;
        node->data.ternary_op.false_value = false_value;
        return node;
    }
    if (parser->current_token.type == TOKEN_NULL_COALESCING) {
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        parser_advance(parser);
        ASTNode* right = parser_parse_binary_operation(parser, 0);
        if (PARSER_UNLIKELY(!right)) {
            ast_destroy_node(condition);
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_NULL_COALESCING, line, col);
        if (PARSER_UNLIKELY(!node)) {
            ast_destroy_node(condition);
            ast_destroy_node(right);
            return NULL;
        }
        node->data.null_coalescing.left = condition;
        node->data.null_coalescing.right = right;
        return node;
    }
    return condition;
}

static ASTNode* parser_parse_expression(Parser* parser) {
    printf("[DEBUG] parser_parse_expression: starting, token=%d, value=%s\n", parser->current_token.type, parser->current_token.value ? parser->current_token.value : "null");
    ASTNode* result = parser_parse_ternary_operation(parser);
    printf("[DEBUG] parser_parse_expression: result=%p, next_token=%d\n", (void*)result, parser->current_token.type);
    return result;
}

static ASTNode* parser_parse_primary_debug(Parser* parser) {
    printf("[DEBUG] parser_parse_primary: token=%d, value=%s\n", parser->current_token.type, parser->current_token.value ? parser->current_token.value : "null");
    ASTNode* result = parser_parse_primary(parser);
    printf("[DEBUG] parser_parse_primary: result=%p, next_token=%d\n", (void*)result, parser->current_token.type);
    return result;
}

static ASTNode* parser_parse_variable_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    int is_let = (parser->current_token.type == TOKEN_LET) ? 1 : 0;
    parser_advance(parser);
    char* name = NULL;
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        name = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
    } else if (parser->current_token.type >= TOKEN_OP_ADDITION &&
               parser->current_token.type <= TOKEN_OP_EXPLICIT) {
        const char* op_name = token_type_to_string(parser->current_token.type);
        name = KRT_STRDUP(op_name ? op_name : "op_Unknown");
        parser_advance(parser);
    } else {
        return NULL;
    }
    if (!name) {
        return NULL;
    }
    ASTNode* array_size = NULL;
    bool is_array = false;
    if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);
        is_array = true;
        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            array_size = parser_parse_expression(parser);
            if (!array_size) {
                KRT_FREE(name);
                return NULL;
            }
        }
        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            KRT_FREE(name);
            if (array_size) {
                ast_destroy_node(array_size);
            }
            return NULL;
        }
        parser_advance(parser);
    }
    KrtTokenType type_token = TOKEN_UNKNOWN;
    char* type_name = NULL;
    if (parser->current_token.type == TOKEN_COLON) {
        parser_advance(parser);
        if (parser_is_type_keyword(parser->current_token.type)) {
            type_token = parser->current_token.type;
            parser_advance(parser);
        } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
            type_token = TOKEN_IDENTIFIER;
            type_name = KRT_STRDUP(parser->current_token.value);
            if (!type_name) {
                KRT_FREE(name);
                if (array_size) {
                    ast_destroy_node(array_size);
                }
                return NULL;
            }
            parser_advance(parser);
        } else {
            KRT_FREE(name);
            if (array_size) {
                ast_destroy_node(array_size);
            }
            return NULL;
        }
    }
    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);
        value = parser_parse_expression(parser);
        if (!value) {
            KRT_FREE(name);
            KRT_FREE(type_name);
            if (array_size) {
                ast_destroy_node(array_size);
            }
            return NULL;
        }
    }
    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION, line, col);
    if (!node) {
        KRT_FREE(name);
        KRT_FREE(type_name);
        if (value) {
            ast_destroy_node(value);
        }
        if (array_size) {
            ast_destroy_node(array_size);
        }
        return NULL;
    }
    node->data.variable_decl.name = name;
    node->data.variable_decl.value = value;
    node->data.variable_decl.type = type_token;
    node->data.variable_decl.template_instantiation_type = type_name;
    node->data.variable_decl.array_size = array_size;
    node->data.variable_decl.is_array = is_array;
    node->data.variable_decl.is_let = is_let;
    return node;
}

static ASTNode* parser_parse_static_member_declaration(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_STATIC) {
        return NULL;
    }
    parser_advance(parser);
    if (parser->current_token.type == TOKEN_FUNCTION) {
        int func_line = parser->current_token.line;
        int func_col = parser->current_token.column;
        parser_advance(parser);
        
        KrtTokenType return_type_token = TOKEN_UNKNOWN;
        if (parser_is_type_keyword(parser->current_token.type)) {
            return_type_token = parser->current_token.type;
            parser_advance(parser);
        }
        
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            return NULL;
        }
        char* name = KRT_STRDUP(parser->current_token.value);
        if (!name) {
            return NULL;
        }
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_LEFT_PAREN) {
            KRT_FREE(name);
            return NULL;
        }
        parser_advance(parser);
        char** parameters = NULL;
        KrtTokenType* parameter_types = NULL;
        int* parameter_is_params = NULL;
        int* parameter_is_array = NULL;
        ASTNode** parameter_default_values = NULL;
        int parameter_count = 0;
        if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_is_params, &parameter_is_array, &parameter_default_values, &parameter_count)) {
            KRT_FREE(name);
            return NULL;
        }
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
        parser_advance(parser);
        
        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type)) {
                parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
                KRT_FREE(name);
                return NULL;
            }
            
            return_type_token = parser->current_token.type;
            parser_advance(parser);
        }
        ASTNode* body = NULL;
        if (parser->current_token.type == TOKEN_LEFT_BRACE) {
            body = parser_parse_block(parser);
            if (!body) {
                parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
                KRT_FREE(name);
                return NULL;
            }
        } else if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        } else {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
        ASTNode* node = parser_create_function_node(parser, 1, 0, name,
                                                parameters,
                                                parameter_types,
                                                parameter_is_params,
                                                parameter_is_array,
                                                parameter_default_values,
                                                parameter_count,
                                                body,
                                                return_type_token,
                                                func_line, func_col);
        if (!node) {
            if (body) {
                ast_destroy_node(body);
            }
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
        return node;
    }
    if (parser_is_type_keyword(parser->current_token.type)) {
        int var_line = parser->current_token.line;
        int var_col = parser->current_token.column;
        KrtTokenType type_token = parser->current_token.type;
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            return NULL;
        }
        char* identifier_name = KRT_STRDUP(parser->current_token.value);
        if (!identifier_name) {
            return NULL;
        }
        Token next_token = lexer_peek_token(parser->lexer);
        KrtTokenType next_type = next_token.type;
        token_free(&next_token);
        if (next_type == TOKEN_LEFT_PAREN) {
            parser_advance(parser);
            return parser_parse_typed_function_declaration(parser, type_token, identifier_name, 1, var_line, var_col);
        }
        parser_advance(parser);
        ASTNode* value = NULL;
        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            value = parser_parse_expression(parser);
            if (!value) {
                KRT_FREE(identifier_name);
                return NULL;
            }
        }
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
        ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION, var_line, var_col);
        node->data.static_variable_decl.name = identifier_name;
        node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = type_token;
        return node;
    }
    if (parser->current_token.type == TOKEN_VAR) {
        int var_line = parser->current_token.line;
        int var_col = parser->current_token.column;
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            return NULL;
        }
        char* name = KRT_STRDUP(parser->current_token.value);
        if (!name) {
            return NULL;
        }
        parser_advance(parser);

        // Parse type annotation if present
        KrtTokenType type_token = TOKEN_UNKNOWN;
        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (parser_is_type_keyword(parser->current_token.type) || parser->current_token.type == TOKEN_IDENTIFIER) {
                type_token = parser->current_token.type;
                parser_advance(parser);
            } else {
                KRT_FREE(name);
                return NULL;
            }
        }

        ASTNode* value = NULL;
        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            value = parser_parse_expression(parser);
            if (!value) {
                KRT_FREE(name);
                return NULL;
            }
        }
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
        ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION, var_line, var_col);
        if (!node) {
            KRT_FREE(name);
            if (value) {
                ast_destroy_node(value);
            }
            return NULL;
        }
        node->data.static_variable_decl.name = name;
        node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = type_token;
        return node;
    }
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        int var_line = parser->current_token.line;
        int var_col = parser->current_token.column;
        char* name = KRT_STRDUP(parser->current_token.value);
        if (!name) {
            return NULL;
        }
        parser_advance(parser);
        ASTNode* value = NULL;
        if (parser->current_token.type == TOKEN_ASSIGN) {
            parser_advance(parser);
            value = parser_parse_expression(parser);
            if (!value) {
                KRT_FREE(name);
                return NULL;
            }
        }
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
        ASTNode* node = ast_create_node(AST_STATIC_VARIABLE_DECLARATION, var_line, var_col);
        if (!node) {
            KRT_FREE(name);
            if (value) {
                ast_destroy_node(value);
            }
            return NULL;
        }
        node->data.static_variable_decl.name = name;
        node->data.static_variable_decl.value = value;
        node->data.static_variable_decl.type = TOKEN_UNKNOWN;
        return node;
    }
    return NULL;
}

static ASTNode* parser_parse_assignment_from_left(Parser* parser, ASTNode* left) {
    if (!left) return NULL;
    
    KrtTokenType operator = parser->current_token.type;
    if (operator != TOKEN_ASSIGN && 
        operator != TOKEN_PLUS_ASSIGN && 
        operator != TOKEN_MINUS_ASSIGN && 
        operator != TOKEN_MUL_ASSIGN && 
        operator != TOKEN_DIV_ASSIGN && 
        operator != TOKEN_MOD_ASSIGN) {
        return left;
    }
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        ast_destroy_node(left);
        return NULL;
    }
    
    if (left->type == AST_ARRAY_ACCESS) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_ARRAY_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.array_assignment.array = left->data.array_access.array;
            node->data.array_assignment.index = left->data.array_access.index;
            node->data.array_assignment.value = value;
            KRT_FREE(left);
            return node;
        } else {
            ASTNode* node = ast_create_node(AST_ARRAY_COMPOUND_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.array_compound_assignment.array = left->data.array_access.array;
            node->data.array_compound_assignment.index = left->data.array_access.index;
            node->data.array_compound_assignment.value = value;
            node->data.array_compound_assignment.operator = operator;
            KRT_FREE(left);
            return node;
        }
    } else if (left->type == AST_IDENTIFIER) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.assignment.value = value;
            ast_destroy_node(left);
            return node;
        } else {
            ASTNode* node = ast_create_node(AST_COMPOUND_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.compound_assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.compound_assignment.value = value;
            node->data.compound_assignment.operator = operator;
            ast_destroy_node(left);
            return node;
        }
    } else if (left->type == AST_MEMBER_ACCESS) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_BINARY_OPERATION, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.binary_op.left = left;
            node->data.binary_op.operator = operator;
            node->data.binary_op.right = value;
            return node;
        }
        ast_destroy_node(left);
        ast_destroy_node(value);
        return NULL;
    }

    ast_destroy_node(left);
    ast_destroy_node(value);
    return NULL;
}

static ASTNode* parser_parse_assignment(Parser* parser) {
    ASTNode* left = parser_parse_expression(parser);
    if (!left) {
        return NULL;
    }
    KrtTokenType operator = parser->current_token.type;
    if (operator != TOKEN_ASSIGN && 
        operator != TOKEN_PLUS_ASSIGN && 
        operator != TOKEN_MINUS_ASSIGN && 
        operator != TOKEN_MUL_ASSIGN && 
        operator != TOKEN_DIV_ASSIGN && 
        operator != TOKEN_MOD_ASSIGN) {
        ast_destroy_node(left);
        return NULL;
    }
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        ast_destroy_node(left);
        return NULL;
    }
    if (left->type == AST_ARRAY_ACCESS) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_ARRAY_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.array_assignment.array = left->data.array_access.array;
            node->data.array_assignment.index = left->data.array_access.index;
            node->data.array_assignment.value = value;
            KRT_FREE(left);
            return node;
        } else {
            ASTNode* node = ast_create_node(AST_ARRAY_COMPOUND_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.array_compound_assignment.array = left->data.array_access.array;
            node->data.array_compound_assignment.index = left->data.array_access.index;
            node->data.array_compound_assignment.value = value;
            node->data.array_compound_assignment.operator = operator;
            KRT_FREE(left);
            return node;
        }
    } else if (left->type == AST_IDENTIFIER) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.assignment.value = value;
            ast_destroy_node(left);
            return node;
        } else {
            ASTNode* node = ast_create_node(AST_COMPOUND_ASSIGNMENT, line, col);
            if (!node) {
                ast_destroy_node(left);
                ast_destroy_node(value);
                return NULL;
            }
            node->data.compound_assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.compound_assignment.value = value;
            node->data.compound_assignment.operator = operator;
            ast_destroy_node(left);
            return node;
        }
    } else {
        ast_destroy_node(left);
        ast_destroy_node(value);
        return NULL;
    }
}

static ASTNode* parser_parse_print_statement(Parser* parser) {
    parser_advance(parser); 
    return parser_parse_print_like_statement(parser, true);
}

static ASTNode* parser_parse_return_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* value = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        value = parser_parse_expression(parser);
        if (!value) {
            return NULL;
        }
    }
    ASTNode* node = ast_create_node(AST_RETURN_STATEMENT, line, col);
    if (!node) {
        if (value) {
            ast_destroy_node(value);
        }
        return NULL;
    }
    node->data.return_stmt.value = value;
    return node;
}

static ASTNode* parser_parse_access_modifier(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    KrtTokenType access_type = parser->current_token.type;
    parser_advance(parser);
    int is_virtual = 0;
    if (parser->current_token.type == TOKEN_VIRTUAL) {
        is_virtual = 1;
        parser_advance(parser);
    }
    if (parser->current_token.type == TOKEN_STATIC) {
        ASTNode* member = parser_parse_static_member_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        return node;
    } else if (parser->current_class && 
               parser->current_token.type == TOKEN_IDENTIFIER &&
               strcmp(parser->current_token.value, parser->current_class) == 0) {
        ASTNode* member = parser_parse_constructor_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        return node;
    } else if (parser->current_class && 
               parser->current_token.type == TOKEN_TILDE) {
        
        Token peek = lexer_peek_token(parser->lexer);
        if (peek.type == TOKEN_IDENTIFIER && strcmp(peek.value, parser->current_class) == 0) {
            token_free(&peek);
            ASTNode* member = parser_parse_destructor_declaration(parser);
            if (!member) {
                return NULL;
            }
            ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
            if (!node) {
                ast_destroy_node(member);
                return NULL;
            }
            node->data.access_modifier.access_modifier = access_type;
            node->data.access_modifier.member = member;
            node->data.access_modifier.is_virtual = is_virtual;
            return node;
        }
        token_free(&peek);
        return NULL;
    } else if (parser_is_type_keyword(parser->current_token.type)) {
        ASTNode* member = parser_parse_type_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        return node;
    } else if (parser->current_token.type == TOKEN_FUNCTION) {
        ASTNode* member = NULL;
        if (parser->current_class) {
            Token peek = lexer_peek_token(parser->lexer);
            if (peek.type == TOKEN_IDENTIFIER && strcmp(peek.value, parser->current_class) == 0) {
                token_free(&peek);
                member = parser_parse_constructor_declaration(parser);
            } else {
                token_free(&peek);
            }
        }
        if (!member) {
            member = parser_parse_function_declaration(parser);
        }
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        return node;
    } else if (parser->current_token.type == TOKEN_VAR || parser->current_token.type == TOKEN_LET) {
        ASTNode* member = parser_parse_variable_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        return node;
    } else if (parser->current_token.type == TOKEN_CLASS || parser->current_token.type == TOKEN_STRUCT) {
        printf("[DEBUG] parser_parse_access_modifier: parsing class/struct, token=%d\n", parser->current_token.type);
        ASTNode* member = parser_parse_class_declaration(parser);
        printf("[DEBUG] parser_parse_access_modifier: class_declaration returned=%p, type=%d\n", (void*)member, member ? (int)member->type : -1);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        printf("[DEBUG] parser_parse_access_modifier: created access_modifier node=%p, member=%p, member_type=%d\n", (void*)node, (void*)member, member->type);
        return node;
    } else if (parser->current_token.type == TOKEN_DELEGATE) {
        printf("[DEBUG] parser_parse_access_modifier: parsing delegate, token=%d\n", parser->current_token.type);
        ASTNode* member = parser_parse_delegate_declaration(parser);
        printf("[DEBUG] parser_parse_access_modifier: delegate_declaration returned=%p, type=%d\n", (void*)member, member ? (int)member->type : -1);
        if (!member) {
            return NULL;
        }
        ASTNode* node = PARSER_CREATE_NODE(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        node->data.access_modifier.is_virtual = is_virtual;
        printf("[DEBUG] parser_parse_access_modifier: created access_modifier node=%p, member=%p, member_type=%d\n", (void*)node, (void*)member, member->type);
        return node;
    }
    return NULL;
}

static ASTNode* parser_parse_constructor_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    if (parser->current_token.type == TOKEN_FUNCTION) {
        parser_advance(parser);
    }
    if (parser->current_token.type != TOKEN_IDENTIFIER ||
        !parser->current_class ||
        strcmp(parser->current_token.value, parser->current_class) != 0) {
        return NULL;
    }
    parser_advance(parser);
    char** parameters = NULL;
    KrtTokenType* parameter_types = NULL;
    int* parameter_is_params = NULL;
    int* parameter_is_array = NULL;
    int parameter_count = 0;
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        while (1) {
            KrtTokenType param_type = TOKEN_UNKNOWN;
            if (parser_is_type_keyword(parser->current_token.type)) {
                param_type = parser->current_token.type;
                parser_advance(parser);
            }
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                return NULL;
            }
            char** new_parameters = KRT_REALLOC(parameters, (parameter_count + 1) * sizeof(char*));
            KrtTokenType* new_parameter_types = KRT_REALLOC(parameter_types, (parameter_count + 1) * sizeof(KrtTokenType));
            int* new_is_params = KRT_REALLOC(parameter_is_params, (parameter_count + 1) * sizeof(int));
            if (!new_parameters || !new_parameter_types || !new_is_params) {
                KRT_FREE(parser->current_token.value);
                parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                return NULL;
            }
            parameters = new_parameters;
            parameter_types = new_parameter_types;
            parameter_is_params = new_is_params;
            parameters[parameter_count] = KRT_STRDUP(parser->current_token.value);
            parameter_types[parameter_count] = param_type;
            parameter_is_params[parameter_count] = 0;
            parameter_count++;
            parser_advance(parser);
            if (parser->current_token.type == TOKEN_COLON) {
                parser_advance(parser);
                if (parser_is_type_keyword(parser->current_token.type)) {
                    parameter_types[parameter_count - 1] = parser->current_token.type;
                    parser_advance(parser);
                } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
                    parameter_types[parameter_count - 1] = TOKEN_UNKNOWN;
                    parser_advance(parser);
                } else {
                    parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                    return NULL;
                }
            }
            if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                break;
            }
            if (parser->current_token.type != TOKEN_COMMA) {
                parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                return NULL;
            }
            parser_advance(parser);
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
        return NULL;
    }
    parser_advance(parser);

    ASTNode** base_arguments = NULL;
    int base_argument_count = 0;
    int has_base_call = 0;
    if (parser->current_token.type == TOKEN_COLON) {
        has_base_call = 1;
        parser_advance(parser);
        if ((parser->current_token.type != TOKEN_BASE) &&
            (parser->current_token.type != TOKEN_IDENTIFIER || strcmp(parser->current_token.value, "base") != 0)) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
            return NULL;
        }
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_LEFT_PAREN) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
            return NULL;
        }
        parser_advance(parser);
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            while (1) {
                ASTNode* arg = parser_parse_expression(parser);
                if (!arg) {
                    parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                    for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
                    KRT_FREE(base_arguments);
                    return NULL;
                }
                ASTNode** new_args = KRT_REALLOC(base_arguments, (base_argument_count + 1) * sizeof(ASTNode*));
                if (!new_args) {
                    parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                    ast_destroy_node(arg);
                    for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
                    KRT_FREE(base_arguments);
                    return NULL;
                }
                base_arguments = new_args;
                base_arguments[base_argument_count++] = arg;
                if (parser->current_token.type == TOKEN_RIGHT_PAREN) break;
                if (parser->current_token.type != TOKEN_COMMA) {
                    parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
                    for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
                    KRT_FREE(base_arguments);
                    return NULL;
                }
                parser_advance(parser);
            }
        }
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
            for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
            KRT_FREE(base_arguments);
            return NULL;
        }
        parser_advance(parser);
    }

    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
            for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
            KRT_FREE(base_arguments);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
        for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
        KRT_FREE(base_arguments);
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_CONSTRUCTOR_DECLARATION, line, col);
    if (!node) {
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, NULL, parameter_count);
        for (int i = 0; i < base_argument_count; i++) ast_destroy_node(base_arguments[i]);
        KRT_FREE(base_arguments);
        if (body) {
            ast_destroy_node(body);
        }
        return NULL;
    }
    node->data.constructor_decl.parameters = parameters;
    node->data.constructor_decl.parameter_types = parameter_types;
    node->data.constructor_decl.parameter_count = parameter_count;
    node->data.constructor_decl.body = body;
    node->data.constructor_decl.base_arguments = base_arguments;
    node->data.constructor_decl.base_argument_count = base_argument_count;
    node->data.constructor_decl.has_base_call = has_base_call;
    return node;
}

static ASTNode* parser_parse_destructor_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser); 
    
    if (parser->current_token.type != TOKEN_IDENTIFIER || 
        strcmp(parser->current_token.value, parser->current_class) != 0) {
        return NULL;
    }
    char* class_name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser); 
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        KRT_FREE(class_name);
        return NULL;
    }
    parser_advance(parser); 
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        KRT_FREE(class_name);
        return NULL;
    }
    parser_advance(parser); 
    
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            if (class_name) {
                KRT_FREE(class_name);
            }
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        if (class_name) {
            KRT_FREE(class_name);
        }
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_DESTRUCTOR_DECLARATION, line, col);
    if (!node) {
        if (class_name) {
            KRT_FREE(class_name);
        }
        if (body) {
            ast_destroy_node(body);
        }
        return NULL;
    }
    node->data.destructor_decl.class_name = class_name;
    node->data.destructor_decl.body = body;
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    return node;
}

static ASTNode* parser_parse_function_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    printf("[DEBUG] parser_parse_function_declaration: starting, token=%d, value=%s\n", parser->current_token.type, parser->current_token.value ? parser->current_token.value : "null");
    
    // Check for async modifier
    int is_async = 0;
    if (parser->current_token.type == TOKEN_ASYNC) {
        is_async = 1;
        parser_advance(parser);
    }
    
    if (parser->current_token.type != TOKEN_FUNCTION) {
        return NULL;
    }
    parser_advance(parser);

    KrtTokenType return_type = TOKEN_INT32;  
    if (parser->current_token.type == TOKEN_VOID ||
        parser->current_token.type == TOKEN_INT8 ||
        parser->current_token.type == TOKEN_INT16 ||
        parser->current_token.type == TOKEN_INT32 ||
        parser->current_token.type == TOKEN_INT64 ||
        parser->current_token.type == TOKEN_UINT8 ||
        parser->current_token.type == TOKEN_UINT16 ||
        parser->current_token.type == TOKEN_UINT32 ||
        parser->current_token.type == TOKEN_UINT64 ||
        parser->current_token.type == TOKEN_FLOAT32 ||
        parser->current_token.type == TOKEN_FLOAT64 ||
        parser->current_token.type == TOKEN_BOOL ||
        parser->current_token.type == TOKEN_CHAR ||
        parser->current_token.type == TOKEN_TYPE_STRING) {
        return_type = parser->current_token.type;
        parser_advance(parser);
    }

    char* name = NULL;
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        name = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
    } else if (parser->current_token.type >= TOKEN_OP_ADDITION &&
               parser->current_token.type <= TOKEN_OP_EXPLICIT) {
        const char* op_name = token_type_to_string(parser->current_token.type);
        name = KRT_STRDUP(op_name ? op_name : "op_Unknown");
        parser_advance(parser);
    } else {
        printf("[DEBUG] parser_parse_function_declaration: expected IDENTIFIER or OPERATOR, got %d\n", parser->current_token.type);
        return NULL;
    }
    if (!name) {
        return NULL;
    }
    printf("[DEBUG] parser_parse_function_declaration: name=%s, next_token=%d\n", name, parser->current_token.type);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        printf("[DEBUG] parser_parse_function_declaration: expected LEFT_PAREN, got %d\n", parser->current_token.type);
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    char** parameters = NULL;
    KrtTokenType* parameter_types = NULL;
    int* parameter_is_params = NULL;
    int* parameter_is_array = NULL;
    ASTNode** parameter_default_values = NULL;
    int parameter_count = 0;
    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_is_params, &parameter_is_array, &parameter_default_values, &parameter_count)) {
        printf("[DEBUG] parser_parse_function_declaration: parameter_list parsing failed\n");
        KRT_FREE(name);
        return NULL;
    }
    printf("[DEBUG] parser_parse_function_declaration: parameter_count=%d\n", parameter_count);
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        printf("[DEBUG] parser_parse_function_declaration: expected RIGHT_PAREN, got %d\n", parser->current_token.type);
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    printf("[DEBUG] parser_parse_function_declaration: after params, token=%d\n", parser->current_token.type);
    
    // Support return type declaration: function Name(): type
    if (parser->current_token.type == TOKEN_COLON) {
        parser_advance(parser);
        if (parser->current_token.type == TOKEN_VOID ||
            parser->current_token.type == TOKEN_INT8 ||
            parser->current_token.type == TOKEN_INT16 ||
            parser->current_token.type == TOKEN_INT32 ||
            parser->current_token.type == TOKEN_INT64 ||
            parser->current_token.type == TOKEN_UINT8 ||
            parser->current_token.type == TOKEN_UINT16 ||
            parser->current_token.type == TOKEN_UINT32 ||
            parser->current_token.type == TOKEN_UINT64 ||
            parser->current_token.type == TOKEN_FLOAT32 ||
            parser->current_token.type == TOKEN_FLOAT64 ||
            parser->current_token.type == TOKEN_BOOL ||
            parser->current_token.type == TOKEN_CHAR ||
            parser->current_token.type == TOKEN_TYPE_STRING ||
            parser->current_token.type == TOKEN_IDENTIFIER) {
            return_type = parser->current_token.type;
            parser_advance(parser);
        } else {
            printf("[DEBUG] parser_parse_function_declaration: expected return type after :, got %d\n", parser->current_token.type);
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
    }
    
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            printf("[DEBUG] parser_parse_function_declaration: block parsing failed\n");
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        printf("[DEBUG] parser_parse_function_declaration: expected LEFT_BRACE or SEMICOLON, got %d\n", parser->current_token.type);
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* node = parser_create_function_node(parser, 0, is_async, name,
                                                parameters,
                                                parameter_types,
                                                parameter_is_params,
                                                parameter_is_array,
                                                parameter_default_values,
                                                parameter_count,
                                                body,
                                                return_type,
                                                line, col);
    if (!node) {
        if (body) {
            ast_destroy_node(body);
        }
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(name);
        printf("[DEBUG] parser_parse_function_declaration: parser_create_function_node failed\n");
        return NULL;
    }
    printf("[DEBUG] parser_parse_function_declaration: created node=%p, name=%s\n", (void*)node, name);
    return node;
}

static ASTNode* parser_parse_typed_function_declaration(Parser* parser,
                                                       KrtTokenType return_type,
                                                       char* function_name,
                                                       int is_static,
                                                       int line, int col) {
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        KRT_FREE(function_name);
        return NULL;
    }
    parser_advance(parser);
    char** parameters = NULL;
    KrtTokenType* parameter_types = NULL;
    int* parameter_is_params = NULL;
    int* parameter_is_array = NULL;
    ASTNode** parameter_default_values = NULL;
    int parameter_count = 0;
    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_is_params, &parameter_is_array, &parameter_default_values, &parameter_count)) {
        KRT_FREE(function_name);
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(function_name);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
            KRT_FREE(function_name);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(function_name);
        return NULL;
    }
    ASTNode* node = parser_create_function_node(parser,
                                                is_static,
                                                0,  // is_async
                                                function_name,
                                                parameters,
                                                parameter_types,
                                                parameter_is_params,
                                                parameter_is_array,
                                                parameter_default_values,
                                                parameter_count,
                                                body,
                                                return_type,
                                                line, col);
    if (!node) {
        if (body) {
            ast_destroy_node(body);
        }
        parser_free_parameter_list(parameters, parameter_types, parameter_is_params, parameter_is_array, parameter_default_values, parameter_count);
        KRT_FREE(function_name);
        return NULL;
    }
    return node;
}
static ASTNode* parser_parse_if_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) {
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* then_branch = parser_parse_statement(parser);
    if (!then_branch) {
        ast_destroy_node(condition);
        return NULL;
    }
    ASTNode* else_branch = NULL;
    if (parser->current_token.type == TOKEN_ELSE) {
        parser_advance(parser);
        else_branch = parser_parse_statement(parser);
        if (!else_branch) {
            ast_destroy_node(condition);
            ast_destroy_node(then_branch);
            return NULL;
        }
    }
    ASTNode* node = ast_create_node(AST_IF_STATEMENT, line, col);
    if (!node) {
        ast_destroy_node(condition);
        ast_destroy_node(then_branch);
        if (else_branch) {
            ast_destroy_node(else_branch);
        }
        return NULL;
    }
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    return node;
}

static ASTNode* parser_parse_while_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) {
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        ast_destroy_node(condition);
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_WHILE_STATEMENT, line, col);
    if (!node) {
        ast_destroy_node(condition);
        ast_destroy_node(body);
        return NULL;
    }
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

static ASTNode* parser_parse_for_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode* init = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (parser->current_token.type == TOKEN_VAR || parser->current_token.type == TOKEN_LET) {
            init = parser_parse_variable_declaration(parser);
        } else if (parser_is_type_keyword(parser->current_token.type)) {
            // Typed variable declaration in for-init: Type name [= expr]
            int decl_line = parser->current_token.line;
            int decl_col = parser->current_token.column;
            KrtTokenType type_token = parser->current_token.type;
            parser_advance(parser);
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                return NULL;
            }
            char* name = KRT_STRDUP(parser->current_token.value);
            if (!name) {
                return NULL;
            }
            parser_advance(parser);
            ASTNode* value = NULL;
            if (parser->current_token.type == TOKEN_ASSIGN) {
                parser_advance(parser);
                value = parser_parse_expression(parser);
                if (!value) {
                    KRT_FREE(name);
                    return NULL;
                }
            }
            init = ast_create_node(AST_VARIABLE_DECLARATION, decl_line, decl_col);
            if (!init) {
                KRT_FREE(name);
                if (value) ast_destroy_node(value);
                return NULL;
            }
            init->data.variable_decl.name = name;
            init->data.variable_decl.value = value;
            init->data.variable_decl.type = type_token;
            init->data.variable_decl.array_size = NULL;
            init->data.variable_decl.is_array = false;
        } else {

            Token next = lexer_peek_token(parser->lexer);
            if (next.type == TOKEN_ASSIGN || 
                next.type == TOKEN_PLUS_ASSIGN || 
                next.type == TOKEN_MINUS_ASSIGN || 
                next.type == TOKEN_MUL_ASSIGN || 
                next.type == TOKEN_DIV_ASSIGN || 
                next.type == TOKEN_MOD_ASSIGN) {
                token_free(&next);
                init = parser_parse_assignment(parser);
            } else {
                token_free(&next);
                init = parser_parse_expression(parser);
            }
        }
        if (!init) {
            return NULL;
        }
    }
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (init) {
            ast_destroy_node(init);
        }
        return NULL;
    }
    parser_advance(parser);
    ASTNode* condition = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        condition = parser_parse_expression(parser);
        if (!condition) {
            if (init) {
                ast_destroy_node(init);
            }
            return NULL;
        }
    }
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (init) {
            ast_destroy_node(init);
        }
        if (condition) {
            ast_destroy_node(condition);
        }
        return NULL;
    }
    parser_advance(parser);
    ASTNode* increment = NULL;
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        Token next = lexer_peek_token(parser->lexer);
        if (next.type == TOKEN_ASSIGN || 
            next.type == TOKEN_PLUS_ASSIGN || 
            next.type == TOKEN_MINUS_ASSIGN || 
            next.type == TOKEN_MUL_ASSIGN || 
            next.type == TOKEN_DIV_ASSIGN || 
            next.type == TOKEN_MOD_ASSIGN) {
            token_free(&next);
            increment = parser_parse_assignment(parser);
        } else {
            token_free(&next);
            increment = parser_parse_expression(parser);
        }
        
        if (!increment) {
            if (init) {
                ast_destroy_node(init);
            }
            if (condition) {
                ast_destroy_node(condition);
            }
            return NULL;
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        if (init) {
            ast_destroy_node(init);
        }
        if (condition) {
            ast_destroy_node(condition);
        }
        if (increment) {
            ast_destroy_node(increment);
        }
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        if (init) {
            ast_destroy_node(init);
        }
        if (condition) {
            ast_destroy_node(condition);
        }
        if (increment) {
            ast_destroy_node(increment);
        }
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_FOR_STATEMENT, line, col);
    if (!node) {
        if (init) {
            ast_destroy_node(init);
        }
        if (condition) {
            ast_destroy_node(condition);
        }
        if (increment) {
            ast_destroy_node(increment);
        }
        ast_destroy_node(body);
        return NULL;
    }
    node->data.for_stmt.init = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.increment = increment;
    node->data.for_stmt.body = body;
    return node;
}

static ASTNode* parser_parse_block(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    printf("[DEBUG] parser_parse_block: starting, token=%d\n", parser->current_token.type);
    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        printf("[DEBUG] parser_parse_block: no LEFT_BRACE, parsing statement\n");
        return parser_parse_statement(parser);
    }
    parser_advance(parser);
    ASTNode** statements = NULL;
    int statement_count = 0;
    while (parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {
        printf("[DEBUG] parser_parse_block: parsing statement, token=%d\n", parser->current_token.type);
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            printf("[DEBUG] parser_parse_block: statement parsing failed, token=%d\n", parser->current_token.type);
            for (int i = 0; i < statement_count; i++) {
                ast_destroy_node(statements[i]);
            }
            KRT_FREE(statements);
            return NULL;
        }
        ASTNode** new_statements = (ASTNode**)KRT_REALLOC(statements, (statement_count + 1) * sizeof(ASTNode*));
        if (!new_statements) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy_node(statements[i]);
            }
            ast_destroy_node(stmt);
            KRT_FREE(statements);
            return NULL;
        }
        statements = new_statements;
        statements[statement_count] = stmt;
        statement_count++;
    }
    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        printf("[DEBUG] parser_parse_block: expected RIGHT_BRACE, got %d, statement_count=%d\n", parser->current_token.type, statement_count);
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        KRT_FREE(statements);
        return NULL;
    }
    printf("[DEBUG] parser_parse_block: found RIGHT_BRACE, statement_count=%d\n", statement_count);
    parser_advance(parser);
    ASTNode* node = PARSER_CREATE_NODE(AST_BLOCK, line, col);
    if (!node) {
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        KRT_FREE(statements);
        return NULL;
    }
    node->data.block.statements = statements;
    node->data.block.statement_count = statement_count;
    return node;
}

static ASTNode* parser_parse_statement(Parser* parser) {
    if (!parser) {
        return NULL;
    }
    ASTNode* node = NULL;
    switch (parser->current_token.type) {
        case TOKEN_PUBLIC:
        case TOKEN_PRIVATE:
        case TOKEN_PROTECTED:
            return parser_parse_access_modifier(parser);
        case TOKEN_STATIC:
            return parser_parse_static_member_declaration(parser);
        case TOKEN_VAR:
        case TOKEN_LET:
            node = parser_parse_variable_declaration(parser);
            break;
        case TOKEN_FUNCTION:
            node = parser_parse_function_declaration(parser);
            break;
        
        case TOKEN_RETURN:
            node = parser_parse_return_statement(parser);
            break;
        case TOKEN_YIELD:
            node = parser_parse_yield_statement(parser);
            break;
        case TOKEN_AWAIT:
            node = parser_parse_await_expression(parser);
            break;
        case TOKEN_IF:
            node = parser_parse_if_statement(parser);
            break;
        case TOKEN_WHILE:
            node = parser_parse_while_statement(parser);
            break;
        case TOKEN_FOR:
            node = parser_parse_for_statement(parser);
            break;
        case TOKEN_FOREACH:
            node = parser_parse_foreach_statement(parser);
            break;
        case TOKEN_TRY:
            node = parser_parse_try_statement(parser);
            break;
        case TOKEN_THROW:
            node = parser_parse_throw_statement(parser);
            break;
        case TOKEN_LOCK:
            node = parser_parse_lock_statement(parser);
            break;
        case TOKEN_UNSAFE:
            node = parser_parse_unsafe_call(parser);
            break;
        case TOKEN_SWITCH:
            node = parser_parse_switch_statement(parser);
            break;
        case TOKEN_BREAK:
            node = parser_parse_break_statement(parser);
            break;
        case TOKEN_CONTINUE:
            node = parser_parse_continue_statement(parser);
            break;
        case TOKEN_TEMPLATE:
            node = parser_parse_template_declaration(parser);
            break;
        case TOKEN_NAMESPACE:
            node = parser_parse_namespace_declaration(parser);
            break;
        case TOKEN_USING:
            node = parser_parse_using_directive(parser);
            break;
        case TOKEN_MATCH:
            node = parser_parse_match_expression(parser);
            break;
        case TOKEN_FIXED:
            node = parser_parse_fixed_statement(parser);
            break;
        case TOKEN_CLASS:
        case TOKEN_STRUCT:
            node = parser_parse_class_declaration(parser);
            break;
        case TOKEN_DELEGATE:
            node = parser_parse_delegate_declaration(parser);
            break;
        case TOKEN_INT8:
        case TOKEN_INT16:
        case TOKEN_INT32:
        case TOKEN_INT64:
        case TOKEN_UINT8:
        case TOKEN_UINT16:
        case TOKEN_UINT32:
        case TOKEN_UINT64:
        case TOKEN_FLOAT32:
        case TOKEN_FLOAT64:
        case TOKEN_BOOL:
        case TOKEN_TYPE_STRING:
        case TOKEN_CHAR:
        case TOKEN_VOID:
            node = parser_parse_type_declaration(parser);
            break;
        case TOKEN_STRING:
            node = parser_parse_expression(parser);
            if (parser->current_token.type == TOKEN_SEMICOLON) {
                parser_advance(parser);
            }
            break;
        case TOKEN_LEFT_BRACE:
            node = parser_parse_block(parser);
            break;
        case TOKEN_THIS: {
            ASTNode* left = parser_parse_postfix_expression(parser);
            if (!left) {
                return NULL;
            }
            if (parser->current_token.type == TOKEN_ASSIGN ||
                parser->current_token.type == TOKEN_PLUS_ASSIGN ||
                parser->current_token.type == TOKEN_MINUS_ASSIGN ||
                parser->current_token.type == TOKEN_MUL_ASSIGN ||
                parser->current_token.type == TOKEN_DIV_ASSIGN ||
                parser->current_token.type == TOKEN_MOD_ASSIGN) {
                node = parser_parse_assignment_from_left(parser, left);
            } else {
                ASTNode* expr = left;
                while (parser->current_token.type == TOKEN_PLUS ||
                       parser->current_token.type == TOKEN_MINUS ||
                       parser->current_token.type == TOKEN_MULTIPLY ||
                       parser->current_token.type == TOKEN_DIVIDE ||
                       parser->current_token.type == TOKEN_MODULO ||
                       parser->current_token.type == TOKEN_EQUAL ||
                       parser->current_token.type == TOKEN_NOT_EQUAL ||
                       parser->current_token.type == TOKEN_LESS ||
                       parser->current_token.type == TOKEN_GREATER ||
                       parser->current_token.type == TOKEN_LESS_EQUAL ||
                       parser->current_token.type == TOKEN_GREATER_EQUAL) {
                    KrtTokenType operator = parser->current_token.type;
                    int op_line = parser->current_token.line;
                    int op_col = parser->current_token.column;
                    parser_advance(parser);
                    ASTNode* right = parser_parse_postfix_expression(parser);
                    if (!right) {
                        ast_destroy_node(expr);
                        return NULL;
                    }
                    ASTNode* new_node = ast_create_node(AST_BINARY_OPERATION, op_line, op_col);
                    if (!new_node) {
                        ast_destroy_node(expr);
                        ast_destroy_node(right);
                        return NULL;
                    }
                    new_node->data.binary_op.left = expr;
                    new_node->data.binary_op.operator = operator;
                    new_node->data.binary_op.right = right;
                    expr = new_node;
                }
                node = expr;
            }
            break;
        }
        case TOKEN_IDENTIFIER: {
            if (!parser->is_unsafe_mode && parser->current_token.value[0] == '_' &&
                strcmp(parser->current_token.value, "__lambda_") != 0) { 
                 return NULL;
            }
            if (strcmp(parser->current_token.value, "try") == 0) {
                node = parser_parse_try_statement(parser);
            } else if (strcmp(parser->current_token.value, "catch") == 0) {
                node = parser_parse_catch_clause(parser);
            } else if (strcmp(parser->current_token.value, "throw") == 0) {
                node = parser_parse_throw_statement(parser);
            } else if (strcmp(parser->current_token.value, "console") == 0 ||
                       strcmp(parser->current_token.value, "Console") == 0) {
                node = parser_parse_console_writeline(parser);
                if (!node) {
                    return NULL;
                }
            } else if (strcmp(parser->current_token.value, "print") == 0 ||
                       strcmp(parser->current_token.value, "println") == 0) {
                node = parser_parse_print_statement(parser);
            } else if (strcmp(parser->current_token.value, "delete") == 0) {
                int del_line = parser->current_token.line;
                int del_col = parser->current_token.column;
                parser_advance(parser);
                ASTNode* expr = parser_parse_expression(parser);
                if (!expr) {
                    return NULL;
                }
                ASTNode* delete_node = ast_create_node(AST_DELETE_STATEMENT, del_line, del_col);
                if (!delete_node) {
                    ast_destroy_node(expr);
                    return NULL;
                }
                delete_node->data.delete_stmt.value = expr;
                if (parser->current_token.type == TOKEN_SEMICOLON) {
                    parser_advance(parser);
                }
                return delete_node;
            } else {
                Token next = lexer_peek_token(parser->lexer);
                KrtTokenType next_type = next.type;
                token_free(&next);
                if (next_type == TOKEN_ASSIGN || 
                    next_type == TOKEN_PLUS_ASSIGN || 
                    next_type == TOKEN_MINUS_ASSIGN || 
                    next_type == TOKEN_MUL_ASSIGN || 
                    next_type == TOKEN_DIV_ASSIGN || 
                    next_type == TOKEN_MOD_ASSIGN) {
                    node = parser_parse_assignment(parser);
                } else if (next_type == TOKEN_LEFT_BRACKET) {
                    node = parser_parse_postfix_expression(parser);
                    if (node && (parser->current_token.type == TOKEN_ASSIGN || 
                                parser->current_token.type == TOKEN_PLUS_ASSIGN || 
                                parser->current_token.type == TOKEN_MINUS_ASSIGN || 
                                parser->current_token.type == TOKEN_MUL_ASSIGN || 
                                parser->current_token.type == TOKEN_DIV_ASSIGN || 
                                parser->current_token.type == TOKEN_MOD_ASSIGN)) {
                        node = parser_parse_assignment_from_left(parser, node);
                    } else {
                        ASTNode* left = node;
                        while (parser->current_token.type == TOKEN_PLUS ||
                               parser->current_token.type == TOKEN_MINUS ||
                               parser->current_token.type == TOKEN_MULTIPLY ||
                               parser->current_token.type == TOKEN_DIVIDE ||
                               parser->current_token.type == TOKEN_MODULO ||
                               parser->current_token.type == TOKEN_EQUAL ||
                               parser->current_token.type == TOKEN_NOT_EQUAL ||
                               parser->current_token.type == TOKEN_LESS ||
                               parser->current_token.type == TOKEN_GREATER ||
                               parser->current_token.type == TOKEN_LESS_EQUAL ||
                               parser->current_token.type == TOKEN_GREATER_EQUAL) {
                            KrtTokenType operator = parser->current_token.type;
                            int op_line = parser->current_token.line;
                            int op_col = parser->current_token.column;
                            parser_advance(parser);
                            ASTNode* right = parser_parse_postfix_expression(parser);
                            if (!right) {
                                ast_destroy_node(left);
                                return NULL;
                            }
                            ASTNode* new_node = ast_create_node(AST_BINARY_OPERATION, op_line, op_col);
                            if (!new_node) {
                                ast_destroy_node(left);
                                ast_destroy_node(right);
                                return NULL;
                            }
                            new_node->data.binary_op.left = left;
                            new_node->data.binary_op.operator = operator;
                            new_node->data.binary_op.right = right;
                            left = new_node;
                        }
                        node = left;
                    }
                } else if (next_type == TOKEN_IDENTIFIER) {
                    node = parser_parse_type_declaration(parser);
                } else {
                    node = parser_parse_expression(parser);
                }
            }
            break;
        }
        default:
            if (parser->current_token.type == TOKEN_SEMICOLON) {
                parser_advance(parser);
                return NULL;
            } else if (parser->current_token.type == TOKEN_TEMPLATE) {
                node = parser_parse_template_declaration(parser);
            } else {
                node = parser_parse_expression(parser);
            }
            break;
    }
    if (node && parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    return node;
}
ASTNode* parser_parse(Parser* parser) {
    if (!parser) {
        return NULL;
    }
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    ASTNode** statements = NULL;
    int statement_count = 0;
    int stmt_index = 0;
    while (parser->current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            if (parser->current_token.type != TOKEN_EOF) {
                parser_advance(parser);
            }
            continue;
        }
        ASTNode** new_statements = KRT_REALLOC(statements, (statement_count + 1) * sizeof(ASTNode*));
        if (!new_statements) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy_node(statements[i]);
            }
            KRT_FREE(statements);
            ast_destroy_node(stmt);
            return NULL;
        }
        statements = new_statements;
        statements[statement_count] = stmt;
        statement_count++;
        stmt_index++;
    }
    ASTNode* program = PARSER_CREATE_NODE(AST_PROGRAM, line, col);
    if (!program) {
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        KRT_FREE(statements);
        return NULL;
    }
    program->data.block.statements = statements;
    program->data.block.statement_count = statement_count;
    return program;
}

static ASTNode* parser_parse_new_expression(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* class_name = KRT_STRDUP(parser->current_token.value);
    if (!class_name) {
        return NULL;
    }
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        KRT_FREE(class_name);
        return NULL;
    }
    parser_advance(parser);
    ASTNode** arguments = NULL;
    char** argument_names = NULL;
    int argument_count = 0;
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        while (1) {
            Token next = lexer_peek_token(parser->lexer);
            KrtTokenType next_type = next.type;
            token_free(&next);
            if (parser->current_token.type == TOKEN_IDENTIFIER && next_type == TOKEN_COLON) {
                char* name = KRT_STRDUP(parser->current_token.value);
                parser_advance(parser);
                parser_advance(parser);
                ASTNode* arg = parser_parse_expression(parser);
                if (!arg) {
                    KRT_FREE(name);
                    for (int i = 0; i < argument_count; i++) {
                        ast_destroy_node(arguments[i]);
                        if (argument_names && argument_names[i]) {
                            KRT_FREE(argument_names[i]);
                        }
                    }
                    KRT_FREE(arguments);
                    KRT_FREE(argument_names);
                    KRT_FREE(class_name);
                    return NULL;
                }
                arguments = (ASTNode**)KRT_REALLOC(arguments, (argument_count + 1) * sizeof(ASTNode*));
                argument_names = (char**)KRT_REALLOC(argument_names, (argument_count + 1) * sizeof(char*));
                arguments[argument_count] = arg;
                argument_names[argument_count] = name;
                argument_count++;
            } else {
                ASTNode* arg = parser_parse_expression(parser);
                if (!arg) {
                    for (int i = 0; i < argument_count; i++) {
                        ast_destroy_node(arguments[i]);
                        if (argument_names && argument_names[i]) {
                            KRT_FREE(argument_names[i]);
                        }
                    }
                    KRT_FREE(arguments);
                    KRT_FREE(argument_names);
                    KRT_FREE(class_name);
                    return NULL;
                }
                arguments = (ASTNode**)KRT_REALLOC(arguments, (argument_count + 1) * sizeof(ASTNode*));
                argument_names = (char**)KRT_REALLOC(argument_names, (argument_count + 1) * sizeof(char*));
                arguments[argument_count] = arg;
                argument_names[argument_count] = NULL;
                argument_count++;
            }
            if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                break;
            }
            if (parser->current_token.type != TOKEN_COMMA) {
                for (int i = 0; i < argument_count; i++) {
                    ast_destroy_node(arguments[i]);
                    if (argument_names && argument_names[i]) {
                        KRT_FREE(argument_names[i]);
                    }
                }
                KRT_FREE(arguments);
                KRT_FREE(argument_names);
                KRT_FREE(class_name);
                return NULL;
            }
            parser_advance(parser);
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        for (int i = 0; i < argument_count; i++) {
            ast_destroy_node(arguments[i]);
            if (argument_names && argument_names[i]) {
                KRT_FREE(argument_names[i]);
            }
        }
        KRT_FREE(arguments);
        KRT_FREE(argument_names);
        KRT_FREE(class_name);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* node = ast_create_node(AST_NEW_EXPRESSION, line, col);
    node->data.new_expr.class_name = class_name;
    node->data.new_expr.arguments = arguments;
    node->data.new_expr.argument_count = argument_count;
    node->data.new_expr.argument_names = argument_names;
    return node;
}

ASTNode* parser_parse_try_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* try_block = parser_parse_block(parser);
    if (!try_block) {
        return NULL;
    }
    ASTNode** catch_clauses = NULL;
    int catch_clause_count = 0;
    ASTNode* finally_clause = NULL;
    while (parser->current_token.type == TOKEN_CATCH) {
        ASTNode* catch_clause = parser_parse_catch_clause(parser);
        if (!catch_clause) {
            ast_destroy_node(try_block);
            for (int i = 0; i < catch_clause_count; i++) {
                ast_destroy_node(catch_clauses[i]);
            }
            KRT_FREE(catch_clauses);
            return NULL;
        }
        catch_clauses = (ASTNode**)KRT_REALLOC(catch_clauses, (catch_clause_count + 1) * sizeof(ASTNode*));
        catch_clauses[catch_clause_count] = catch_clause;
        catch_clause_count++;
    }
    if (parser->current_token.type == TOKEN_FINALLY) {
        finally_clause = parser_parse_finally_clause(parser);
        if (!finally_clause) {
            ast_destroy_node(try_block);
            for (int i = 0; i < catch_clause_count; i++) {
                ast_destroy_node(catch_clauses[i]);
            }
            KRT_FREE(catch_clauses);
            return NULL;
        }
    }
    if (catch_clause_count == 0 && !finally_clause) {
        ast_destroy_node(try_block);
        if (catch_clauses) {
            KRT_FREE(catch_clauses);
        }
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_TRY_STATEMENT, line, col);
    if (!node) {
        ast_destroy_node(try_block);
        for (int i = 0; i < catch_clause_count; i++) {
            ast_destroy_node(catch_clauses[i]);
        }
        KRT_FREE(catch_clauses);
        if (finally_clause) {
            ast_destroy_node(finally_clause);
        }
        return NULL;
    }
    node->data.try_stmt.try_block = try_block;
    node->data.try_stmt.catch_clauses = catch_clauses;
    node->data.try_stmt.catch_clause_count = catch_clause_count;
    node->data.try_stmt.finally_clause = finally_clause;
    return node;
}

ASTNode* parser_parse_lock_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* lock_object = parser_parse_expression(parser);
    if (!lock_object) {
        return NULL;
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(lock_object);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        ast_destroy_node(lock_object);
        return NULL;
    }
    
    ASTNode* node = ast_create_node(AST_LOCK_STATEMENT, line, col);
    if (!node) {
        ast_destroy_node(lock_object);
        ast_destroy_node(body);
        return NULL;
    }
    
    node->data.lock_stmt.lock_object = lock_object;
    node->data.lock_stmt.body = body;
    return node;
}

ASTNode* parser_parse_using_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* resource_expr = NULL;
    char* variable_name = NULL;
    
    if (parser->current_token.type == TOKEN_VAR ||
        parser->current_token.type == TOKEN_LET ||
        parser_is_type_keyword(parser->current_token.type)) {
        
        if (parser_is_type_keyword(parser->current_token.type)) {
            parser_advance(parser);
        } else {
            parser_advance(parser);
        }
        
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            return NULL;
        }
        variable_name = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);
        
        if (parser->current_token.type != TOKEN_ASSIGN) {
            KRT_FREE(variable_name);
            return NULL;
        }
        parser_advance(parser);
        
        resource_expr = parser_parse_expression(parser);
        if (!resource_expr) {
            KRT_FREE(variable_name);
            return NULL;
        }
    } else {
        resource_expr = parser_parse_expression(parser);
        if (!resource_expr) {
            return NULL;
        }
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        if (resource_expr) ast_destroy_node(resource_expr);
        if (variable_name) KRT_FREE(variable_name);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        if (resource_expr) ast_destroy_node(resource_expr);
        if (variable_name) KRT_FREE(variable_name);
        return NULL;
    }
    
    ASTNode* node = ast_create_node(AST_USING_STATEMENT, line, col);
    if (!node) {
        if (resource_expr) ast_destroy_node(resource_expr);
        if (variable_name) KRT_FREE(variable_name);
        ast_destroy_node(body);
        return NULL;
    }
    
    node->data.using_stmt.resource = resource_expr;
    node->data.using_stmt.body = body;
    return node;
}

ASTNode* parser_parse_yield_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type == TOKEN_BREAK) {
        parser_advance(parser);
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
        ASTNode* node = ast_create_node(AST_YIELD_BREAK, line, col);
        return node;
    }
    
    if (parser->current_token.type != TOKEN_RETURN) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        return NULL;
    }
    
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    
    ASTNode* node = ast_create_node(AST_YIELD_RETURN, line, col);
    if (!node) {
        ast_destroy_node(value);
        return NULL;
    }
    
    node->data.yield_return.value = value;
    return node;
}

ASTNode* parser_parse_throw_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* exception_expr = NULL;
    int is_rethrow = 0;
    
    // Check if it's a rethrow (throw;) or throw with expression
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        // throw; - rethrow the current exception
        is_rethrow = 1;
        parser_advance(parser);
    } else if (parser->current_token.type != TOKEN_EOF) {
        // throw expr; - throw a new exception
        exception_expr = parser_parse_expression(parser);
        if (!exception_expr) {
            return NULL;
        }
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
    }
    
    ASTNode* node = ast_create_node(AST_THROW_STATEMENT, line, col);
    if (!node) {
        if (exception_expr) {
            ast_destroy_node(exception_expr);
        }
        return NULL;
    }
    node->data.throw_stmt.exception_expr = exception_expr;
    node->data.throw_stmt.is_rethrow = is_rethrow;
    return node;
}

ASTNode* parser_parse_catch_clause(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    char* exception_type = NULL;
    char* exception_var = NULL;
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);
        if (parser->current_token.type == TOKEN_IDENTIFIER ||
            parser->current_token.type == TOKEN_INT8 ||
            parser->current_token.type == TOKEN_INT16 ||
            parser->current_token.type == TOKEN_INT32 ||
            parser->current_token.type == TOKEN_INT64 ||
            parser->current_token.type == TOKEN_STRING ||
            parser->current_token.type == TOKEN_BOOL ||
            parser->current_token.type == TOKEN_FLOAT32 ||
            parser->current_token.type == TOKEN_FLOAT64 ||
            parser->current_token.type == TOKEN_VOID) {
            exception_type = KRT_STRDUP(parser->current_token.value);
            parser_advance(parser);
            if (parser->current_token.type == TOKEN_IDENTIFIER ||
                parser->current_token.type == TOKEN_EXCEPTION) {
                exception_var = KRT_STRDUP(parser->current_token.value);
                parser_advance(parser);
            }
        }
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            if (exception_type) {
                KRT_FREE(exception_type);
            }
            if (exception_var) {
                KRT_FREE(exception_var);
            }
            return NULL;
        }
        parser_advance(parser);
    }
    ASTNode* catch_block = parser_parse_block(parser);
    if (!catch_block) {
        if (exception_type) {
            KRT_FREE(exception_type);
        }
        if (exception_var) {
            KRT_FREE(exception_var);
        }
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_CATCH_CLAUSE, line, col);
    if (!node) {
        if (exception_type) {
            KRT_FREE(exception_type);
        }
        if (exception_var) {
            KRT_FREE(exception_var);
        }
        ast_destroy_node(catch_block);
        return NULL;
    }
    node->data.catch_clause.exception_type = exception_type;
    node->data.catch_clause.exception_var = exception_var;
    node->data.catch_clause.catch_block = catch_block;
    return node;
}

ASTNode* parser_parse_finally_clause(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* finally_block = parser_parse_block(parser);
    if (!finally_block) {
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_FINALLY_CLAUSE, line, col);
    if (!node) {
        ast_destroy_node(finally_block);
        return NULL;
    }
    node->data.finally_clause.finally_block = finally_block;
    return node;
}

ASTNode* parser_parse_template_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LESS) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode** parameters = NULL;
    int parameter_count = 0;
    if (parser->current_token.type != TOKEN_GREATER) {
        while (1) {
            ASTNode* param = parser_parse_template_parameter(parser);
            if (!param) {
                PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
                return NULL;
            }
            parameters = (ASTNode**)KRT_REALLOC(parameters, (parameter_count + 1) * sizeof(ASTNode*));
            parameters[parameter_count] = param;
            parameter_count++;
            if (parser->current_token.type == TOKEN_GREATER) {
                break;
            }
            if (parser->current_token.type != TOKEN_COMMA) {
                PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
                return NULL;
            }
            parser_advance(parser);
        }
    }
    if (parser->current_token.type != TOKEN_GREATER) {
        PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* declaration = NULL;
    if (parser->current_token.type == TOKEN_CLASS) {
        declaration = parser_parse_class_declaration(parser);
    } else if (parser->current_token.type == TOKEN_FUNCTION) {
        declaration = parser_parse_function_declaration(parser);
    } else {
        PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
        return NULL;
    }
    if (!declaration) {
        PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_TEMPLATE_DECLARATION, line, col);
    if (!node) {
        PARSER_FREE_AST_NODE_ARRAY(parameters, parameter_count);
        ast_destroy_node(declaration);
        return NULL;
    }
    node->data.template_decl.parameters = parameters;
    node->data.template_decl.parameter_count = parameter_count;
    node->data.template_decl.declaration = declaration;
    return node;
}

ASTNode* parser_parse_template_parameter(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    int is_typename = 0;
    if (parser->current_token.type == TOKEN_TYPENAME) {
        is_typename = 1;
        parser_advance(parser);
    } else if (parser->current_token.type == TOKEN_CLASS) {
        is_typename = 1;
        parser_advance(parser);
    }
    if (!is_typename) {
        return NULL;
    }
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* param_name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);
    ASTNode* node = ast_create_node(AST_TEMPLATE_PARAMETER, line, col);
    if (!node) {
        KRT_FREE(param_name);
        return NULL;
    }
    node->data.template_param.param_name = param_name;
    return node;
}

static ASTNode* parser_parse_type_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    KrtTokenType type_token = parser->current_token.type;
    int is_nullable = 0;
    int is_array = 0;
    char* type_name = NULL;
    if (type_token == TOKEN_IDENTIFIER) {
        type_name = KRT_STRDUP(parser->current_token.value);
    } else {
        const char* type_str = token_type_to_string(type_token);
        type_name = KRT_STRDUP(type_str ? type_str : "UNKNOWN");
    }
    parser_advance(parser);
    if (parser->current_token.type == TOKEN_QUESTION) {
        is_nullable = 1;
        parser_advance(parser);
    }
    if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
        Token peek = lexer_peek_token(parser->lexer);
        if (peek.type == TOKEN_RIGHT_BRACKET) {
            is_array = 1;
            parser_advance(parser);
            parser_advance(parser);
        }
        token_free(&peek);
    }
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        KRT_FREE(type_name);
        return NULL;
    }
    char* identifier_name = KRT_STRDUP(parser->current_token.value);
    Token next_token = lexer_peek_token(parser->lexer);
    KrtTokenType next_type = next_token.type;
    token_free(&next_token);
    if (next_type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);
        int line = parser->current_token.line;
        int col = parser->current_token.column;
        ASTNode* result = parser_parse_typed_function_declaration(parser, type_token, identifier_name, 0, line, col);
        KRT_FREE(type_name);
        return result;
    }
    
    if (next_type == TOKEN_LEFT_BRACE) {
        parser_advance(parser);
        ASTNode* result = parser_parse_property_declaration(parser, type_token, identifier_name, NULL, 0);
        KRT_FREE(type_name);
        return result;
    }
    parser_advance(parser);
    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);
        value = parser_parse_expression(parser);
        if (!value) {
            KRT_FREE(type_name);
            KRT_FREE(identifier_name);
            return NULL;
        }
    }
    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION, line, col);
    if (!node) {
        KRT_FREE(type_name);
        KRT_FREE(identifier_name);
        if (value) ast_destroy_node(value);
        return NULL;
    }
    node->data.variable_decl.name = identifier_name;
    node->data.variable_decl.value = value;
    node->data.variable_decl.type = type_token;
    node->data.variable_decl.array_size = NULL;
    node->data.variable_decl.is_array = is_array;
    node->data.variable_decl.template_instantiation_type = (type_token == TOKEN_IDENTIFIER && type_name) ? KRT_STRDUP(type_name) : NULL;
    node->data.variable_decl.is_nullable = is_nullable;
    KRT_FREE(type_name);
    
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    return node;
}

ASTNode* parser_parse_switch_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) {
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    ASTNode** cases = NULL;
    int case_count = 0;
    ASTNode* default_case = NULL;
    while (parser->current_token.type != TOKEN_RIGHT_BRACE && parser->current_token.type != TOKEN_EOF) {
        if (parser->current_token.type == TOKEN_CASE) {
            ASTNode* case_node = parser_parse_case_clause(parser);
            if (!case_node) {
                for (int i = 0; i < case_count; i++) {
                    ast_destroy_node(cases[i]);
                }
                if (cases) {
                    KRT_FREE(cases);
                }
                ast_destroy_node(condition);
                return NULL;
            }
            cases = (ASTNode**)KRT_REALLOC(cases, (case_count + 1) * sizeof(ASTNode*));
            cases[case_count] = case_node;
            case_count++;
        } else if (parser->current_token.type == TOKEN_DEFAULT) {
            if (default_case) {
                for (int i = 0; i < case_count; i++) {
                    ast_destroy_node(cases[i]);
                }
                if (cases) {
                    KRT_FREE(cases);
                }
                ast_destroy_node(condition);
                return NULL;
            }
            default_case = parser_parse_default_clause(parser);
            if (!default_case) {
                for (int i = 0; i < case_count; i++) {
                    ast_destroy_node(cases[i]);
                }
                if (cases) {
                    KRT_FREE(cases);
                }
                ast_destroy_node(condition);
                return NULL;
            }
        } else {
            for (int i = 0; i < case_count; i++) {
                ast_destroy_node(cases[i]);
            }
            if (cases) {
                KRT_FREE(cases);
            }
            ast_destroy_node(condition);
            return NULL;
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        for (int i = 0; i < case_count; i++) {
            ast_destroy_node(cases[i]);
        }
        if (cases) {
            KRT_FREE(cases);
        }
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* node = ast_create_node(AST_SWITCH_STATEMENT, line, col);
    if (!node) {
        for (int i = 0; i < case_count; i++) {
            ast_destroy_node(cases[i]);
        }
        if (cases) {
            KRT_FREE(cases);
        }
        if (default_case) {
            ast_destroy_node(default_case);
        }
        ast_destroy_node(condition);
        return NULL;
    }
    node->data.switch_stmt.expression = condition;
    node->data.switch_stmt.cases = cases;
    node->data.switch_stmt.case_count = case_count;
    node->data.switch_stmt.default_case = default_case;
    return node;
}

ASTNode* parser_parse_case_clause(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        return NULL;
    }
    if (parser->current_token.type != TOKEN_COLON) {
        ast_destroy_node(value);
        return NULL;
    }
    parser_advance(parser);
    ASTNode** statements = NULL;
    int statement_count = 0;
    while (parser->current_token.type != TOKEN_CASE &&
           parser->current_token.type != TOKEN_DEFAULT &&
           parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy_node(statements[i]);
            }
            if (statements) {
                KRT_FREE(statements);
            }
            ast_destroy_node(value);
            return NULL;
        }
        statements = (ASTNode**)KRT_REALLOC(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;
    }
    ASTNode* node = ast_create_node(AST_CASE_CLAUSE, line, col);
    if (!node) {
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        if (statements) {
            KRT_FREE(statements);
        }
        ast_destroy_node(value);
        return NULL;
    }
    node->data.case_clause.value = value;
    node->data.case_clause.statements = statements;
    node->data.case_clause.statement_count = statement_count;
    return node;
}

ASTNode* parser_parse_default_clause(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_COLON) {
        return NULL;
    }
    parser_advance(parser);
    ASTNode** statements = NULL;
    int statement_count = 0;
    while (parser->current_token.type != TOKEN_CASE &&
           parser->current_token.type != TOKEN_DEFAULT &&
           parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
            for (int i = 0; i < statement_count; i++) {
                ast_destroy_node(statements[i]);
            }
            if (statements) {
                KRT_FREE(statements);
            }
            return NULL;
        }
        statements = (ASTNode**)KRT_REALLOC(statements, (statement_count + 1) * sizeof(ASTNode*));
        statements[statement_count] = stmt;
        statement_count++;
    }
    ASTNode* node = ast_create_node(AST_DEFAULT_CLAUSE, line, col);
    if (!node) {
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        if (statements) {
            KRT_FREE(statements);
        }
        return NULL;
    }
    node->data.default_clause.statements = statements;
    node->data.default_clause.statement_count = statement_count;
    return node;
}

ASTNode* parser_parse_break_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    return ast_create_node(AST_BREAK_STATEMENT, line, col);
}

ASTNode* parser_parse_continue_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    return ast_create_node(AST_CONTINUE_STATEMENT, line, col);
}

ASTNode* parser_parse_foreach_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    
    if (parser->current_token.type == TOKEN_VAR || parser->current_token.type == TOKEN_LET) {
        parser_advance(parser);
    }
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* var_name = KRT_STRDUP(parser->current_token.value);
    if (!var_name) {
        return NULL;
    }
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_IN) {
        KRT_FREE(var_name);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* iterable = parser_parse_expression(parser);
    if (!iterable) {
        KRT_FREE(var_name);
        return NULL;
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        KRT_FREE(var_name);
        ast_destroy_node(iterable);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        KRT_FREE(var_name);
        ast_destroy_node(iterable);
        return NULL;
    }
    
    ASTNode* node = ast_create_node(AST_FOREACH_STATEMENT, line, col);
    if (!node) {
        KRT_FREE(var_name);
        ast_destroy_node(iterable);
        ast_destroy_node(body);
        return NULL;
    }
    node->data.foreach_stmt.var_name = var_name;
    node->data.foreach_stmt.iterable = iterable;
    node->data.foreach_stmt.body = body;
    return node;
}
static ASTNode* parser_parse_property_declaration(Parser* parser, KrtTokenType type, char* name, ASTNode** attributes, int attribute_count) {
    if (!parser || !name) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* getter = NULL;
    ASTNode* setter = NULL;
    ASTNode* initial_value = NULL;
    
    while (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        if (parser->current_token.type == TOKEN_GET) {
            parser_advance(parser);
            
            if (parser->current_token.type == TOKEN_LAMBDA) {
                
                parser_advance(parser);
                ASTNode* expr = parser_parse_expression(parser);
                if (!expr) {
                    KRT_FREE(name);
                    return NULL;
                }
                getter = ast_create_node(AST_PROPERTY_GETTER, line, col);
                if (!getter) {
                    KRT_FREE(name);
                    ast_destroy_node(expr);
                    return NULL;
                }
                getter->data.property_getter.body = expr;
                
                if (parser->current_token.type == TOKEN_SEMICOLON) {
                    parser_advance(parser);
                }
            } else if (parser->current_token.type == TOKEN_LEFT_BRACE) {
                
                ASTNode* body = parser_parse_block(parser);
                if (!body) {
                    KRT_FREE(name);
                    return NULL;
                }
                getter = ast_create_node(AST_PROPERTY_GETTER, line, col);
                if (!getter) {
                    KRT_FREE(name);
                    ast_destroy_node(body);
                    return NULL;
                }
                getter->data.property_getter.body = body;
            } else if (parser->current_token.type == TOKEN_SEMICOLON) {
                
                parser_advance(parser);
                getter = ast_create_node(AST_PROPERTY_GETTER, line, col);
                if (!getter) {
                    KRT_FREE(name);
                    return NULL;
                }
                getter->data.property_getter.body = NULL;
                getter->data.property_getter.is_auto = 1;
            }
        } else if (parser->current_token.type == TOKEN_SET) {
            parser_advance(parser);
            
            if (parser->current_token.type == TOKEN_LAMBDA) {
                
                parser_advance(parser);
                ASTNode* expr = parser_parse_expression(parser);
                if (!expr) {
                    KRT_FREE(name);
                    ast_destroy_node(getter);
                    return NULL;
                }
                setter = ast_create_node(AST_PROPERTY_SETTER, line, col);
                if (!setter) {
                    KRT_FREE(name);
                    ast_destroy_node(expr);
                    ast_destroy_node(getter);
                    return NULL;
                }
                setter->data.property_setter.value_param_name = KRT_STRDUP("value");
                setter->data.property_setter.body = expr;
                
                if (parser->current_token.type == TOKEN_SEMICOLON) {
                    parser_advance(parser);
                }
            } else if (parser->current_token.type == TOKEN_LEFT_BRACE) {
                
                ASTNode* body = parser_parse_block(parser);
                if (!body) {
                    KRT_FREE(name);
                    ast_destroy_node(getter);
                    return NULL;
                }
                setter = ast_create_node(AST_PROPERTY_SETTER, line, col);
                if (!setter) {
                    KRT_FREE(name);
                    ast_destroy_node(body);
                    ast_destroy_node(getter);
                    return NULL;
                }
                setter->data.property_setter.value_param_name = KRT_STRDUP("value");
                setter->data.property_setter.body = body;
            } else if (parser->current_token.type == TOKEN_SEMICOLON) {
                
                parser_advance(parser);
                setter = ast_create_node(AST_PROPERTY_SETTER, line, col);
                if (!setter) {
                    KRT_FREE(name);
                    ast_destroy_node(getter);
                    return NULL;
                }
                setter->data.property_setter.value_param_name = KRT_STRDUP("value");
                setter->data.property_setter.body = NULL;
                setter->data.property_setter.is_auto = 1;
            }
        } else {
            
            break;
        }
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        KRT_FREE(name);
        ast_destroy_node(getter);
        ast_destroy_node(setter);
        return NULL;
    }
    parser_advance(parser);
    
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);
        initial_value = parser_parse_expression(parser);
        if (!initial_value) {
            KRT_FREE(name);
            ast_destroy_node(getter);
            ast_destroy_node(setter);
            return NULL;
        }
        if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        }
    }
    
    ASTNode* node = ast_create_node(AST_PROPERTY_DECLARATION, line, col);
    if (!node) {
        KRT_FREE(name);
        ast_destroy_node(getter);
        ast_destroy_node(setter);
        ast_destroy_node(initial_value);
        return NULL;
    }
    
    node->data.property_decl.name = name;
    node->data.property_decl.type = type;
    node->data.property_decl.getter = getter;
    node->data.property_decl.setter = setter;
    node->data.property_decl.initial_value = initial_value;
    node->data.property_decl.attributes = attributes;
    node->data.property_decl.attribute_count = attribute_count;
    
    int is_auto = (getter && getter->data.property_getter.is_auto) ||
                  (setter && setter->data.property_setter.is_auto);
    node->data.property_decl.is_auto_property = is_auto;
    
    if (is_auto && name) {
        size_t field_name_len = strlen(name) + 2;
        char* field_name = (char*)KRT_MALLOC(field_name_len);
        if (field_name) {
            field_name[0] = '_';
            strcpy(field_name + 1, name);
            node->data.property_decl.backing_field_name = field_name;
        } else {
            node->data.property_decl.backing_field_name = NULL;
        }
    } else {
        node->data.property_decl.backing_field_name = NULL;
    }
    
    return node;
}
static int parser_parse_lambda_parameters(Parser* parser, char*** parameters, int* parameter_count) {
    if (!parser || !parameters || !parameter_count) return 0;
    
    *parameters = NULL;
    *parameter_count = 0;
    int capacity = 4;
    
    *parameters = (char**)KRT_MALLOC(capacity * sizeof(char*));
    if (!*parameters) return 0;
    
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);
        
        while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                
                for (int i = 0; i < *parameter_count; i++) {
                    KRT_FREE((*parameters)[i]);
                }
                KRT_FREE(*parameters);
                *parameters = NULL;
                *parameter_count = 0;
                return 0;
            }
            
            if (*parameter_count >= capacity) {
                capacity *= 2;
                char** new_params = (char**)KRT_REALLOC(*parameters, capacity * sizeof(char*));
                if (!new_params) {
                    for (int i = 0; i < *parameter_count; i++) {
                        KRT_FREE((*parameters)[i]);
                    }
                    KRT_FREE(*parameters);
                    *parameters = NULL;
                    *parameter_count = 0;
                    return 0;
                }
                *parameters = new_params;
            }
            
            (*parameters)[*parameter_count] = KRT_STRDUP(parser->current_token.value);
            (*parameter_count)++;
            parser_advance(parser);
            
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                
                for (int i = 0; i < *parameter_count; i++) {
                    KRT_FREE((*parameters)[i]);
                }
                KRT_FREE(*parameters);
                *parameters = NULL;
                *parameter_count = 0;
                return 0;
            }
        }
        
        parser_advance(parser); 
    } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
        
        (*parameters)[0] = KRT_STRDUP(parser->current_token.value);
        *parameter_count = 1;
        parser_advance(parser);
    } else {
        KRT_FREE(*parameters);
        *parameters = NULL;
        return 0;
    }
    
    return 1;
}
static ASTNode* parser_parse_lambda_expression(Parser* parser, char** parameters, int parameter_count) {
    if (!parser) return NULL;
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    char** lambda_params = parameters;
    int lambda_param_count = parameter_count;
    
    if (!lambda_params) {
        if (!parser_parse_lambda_parameters(parser, &lambda_params, &lambda_param_count)) {
            return NULL;
        }
    }
    
    if (parser->current_token.type != TOKEN_LAMBDA) {
        for (int i = 0; i < lambda_param_count; i++) {
            KRT_FREE(lambda_params[i]);
        }
        KRT_FREE(lambda_params);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_LAMBDA_EXPRESSION, line, col);
    if (!node) {
        for (int i = 0; i < lambda_param_count; i++) {
            KRT_FREE(lambda_params[i]);
        }
        KRT_FREE(lambda_params);
        return NULL;
    }
    
    node->data.lambda_expr.parameters = lambda_params;
    node->data.lambda_expr.parameter_count = lambda_param_count;
    
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        
        node->data.lambda_expr.body = parser_parse_block(parser);
        node->data.lambda_expr.expression = NULL;
    } else {
        
        node->data.lambda_expr.expression = parser_parse_expression(parser);
        node->data.lambda_expr.body = NULL;
    }
    
    if (!node->data.lambda_expr.body && !node->data.lambda_expr.expression) {
        for (int i = 0; i < lambda_param_count; i++) {
            KRT_FREE(lambda_params[i]);
        }
        KRT_FREE(lambda_params);
        KRT_FREE(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parser_parse_linq_query(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_FROM) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    char* var_name = KRT_STRDUP(parser->current_token.value);
    if (!var_name) return NULL;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_IN) {
        KRT_FREE(var_name);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* source = parser_parse_expression(parser);
    if (!source) {
        KRT_FREE(var_name);
        return NULL;
    }
    
    ASTNode* from_clause = ast_create_node(AST_LINQ_FROM, line, col);
    if (!from_clause) {
        KRT_FREE(var_name);
        ast_destroy_node(source);
        return NULL;
    }
    
    from_clause->data.linq_from.var_name = var_name;
    from_clause->data.linq_from.source = source;
    from_clause->data.linq_from.type = NULL;
    
    int capacity = 4;
    int clause_count = 0;
    ASTNode** clauses = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
    if (!clauses) {
        ast_destroy_node(from_clause);
        return NULL;
    }
    
    ASTNode* select_clause = NULL;
    
    while (1) {
        if (parser->current_token.type == TOKEN_WHERE) {
            parser_advance(parser);
            ASTNode* condition = parser_parse_expression(parser);
            if (!condition) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            ASTNode* where_clause = ast_create_node(AST_LINQ_WHERE, line, col);
            if (!where_clause) {
                ast_destroy_node(condition);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            where_clause->data.linq_where.condition = condition;
            
            if (clause_count >= capacity) {
                capacity *= 2;
                ASTNode** new_clauses = (ASTNode**)KRT_REALLOC(clauses, capacity * sizeof(ASTNode*));
                if (!new_clauses) {
                    ast_destroy_node(where_clause);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                clauses = new_clauses;
            }
            
            clauses[clause_count++] = where_clause;
        } else if (parser->current_token.type == TOKEN_ORDERBY) {
            parser_advance(parser);
            ASTNode* expr = parser_parse_expression(parser);
            if (!expr) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            bool ascending = true;
            if (parser->current_token.type == TOKEN_IDENTIFIER) {
                if (strcmp(parser->current_token.value, "descending") == 0) {
                    ascending = false;
                    parser_advance(parser);
                } else if (strcmp(parser->current_token.value, "ascending") == 0) {
                    parser_advance(parser);
                }
            }
            
            ASTNode* orderby_clause = ast_create_node(AST_LINQ_ORDERBY, line, col);
            if (!orderby_clause) {
                ast_destroy_node(expr);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            orderby_clause->data.linq_orderby.expression = expr;
            orderby_clause->data.linq_orderby.ascending = ascending;
            
            if (clause_count >= capacity) {
                capacity *= 2;
                ASTNode** new_clauses = (ASTNode**)KRT_REALLOC(clauses, capacity * sizeof(ASTNode*));
                if (!new_clauses) {
                    ast_destroy_node(orderby_clause);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                clauses = new_clauses;
            }
            
            clauses[clause_count++] = orderby_clause;
        } else if (parser->current_token.type == TOKEN_JOIN) {
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            char* join_var_name = KRT_STRDUP(parser->current_token.value);
            if (!join_var_name) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_IN) {
                KRT_FREE(join_var_name);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* join_source = parser_parse_expression(parser);
            if (!join_source) {
                KRT_FREE(join_var_name);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_ON) {
                KRT_FREE(join_var_name);
                ast_destroy_node(join_source);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* left_key = parser_parse_expression(parser);
            if (!left_key) {
                KRT_FREE(join_var_name);
                ast_destroy_node(join_source);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_EQUALS) {
                ast_destroy_node(left_key);
                KRT_FREE(join_var_name);
                ast_destroy_node(join_source);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* right_key = parser_parse_expression(parser);
            if (!right_key) {
                ast_destroy_node(left_key);
                KRT_FREE(join_var_name);
                ast_destroy_node(join_source);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            char* into_var_name = NULL;
            if (parser->current_token.type == TOKEN_INTO) {
                parser_advance(parser);
                if (parser->current_token.type != TOKEN_IDENTIFIER) {
                    ast_destroy_node(right_key);
                    ast_destroy_node(left_key);
                    KRT_FREE(join_var_name);
                    ast_destroy_node(join_source);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                into_var_name = KRT_STRDUP(parser->current_token.value);
                if (!into_var_name) {
                    ast_destroy_node(right_key);
                    ast_destroy_node(left_key);
                    KRT_FREE(join_var_name);
                    ast_destroy_node(join_source);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                parser_advance(parser);
            }
            
            ASTNode* join_clause = ast_create_node(AST_LINQ_JOIN, line, col);
            if (!join_clause) {
                if (into_var_name) KRT_FREE(into_var_name);
                ast_destroy_node(right_key);
                ast_destroy_node(left_key);
                KRT_FREE(join_var_name);
                ast_destroy_node(join_source);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            join_clause->data.linq_join.var_name = NULL;
            join_clause->data.linq_join.source = NULL;
            join_clause->data.linq_join.join_var_name = join_var_name;
            join_clause->data.linq_join.join_source = join_source;
            join_clause->data.linq_join.left_key = left_key;
            join_clause->data.linq_join.right_key = right_key;
            join_clause->data.linq_join.into_var_name = into_var_name;
            
            if (clause_count >= capacity) {
                capacity *= 2;
                ASTNode** new_clauses = (ASTNode**)KRT_REALLOC(clauses, capacity * sizeof(ASTNode*));
                if (!new_clauses) {
                    ast_destroy_node(join_clause);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                clauses = new_clauses;
            }
            
            clauses[clause_count++] = join_clause;
        } else if (parser->current_token.type == TOKEN_LET) {
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            char* let_var_name = KRT_STRDUP(parser->current_token.value);
            if (!let_var_name) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            if (parser->current_token.type != TOKEN_ASSIGN) {
                KRT_FREE(let_var_name);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* let_expr = parser_parse_expression(parser);
            if (!let_expr) {
                KRT_FREE(let_var_name);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            ASTNode* let_clause = ast_create_node(AST_LINQ_LET, line, col);
            if (!let_clause) {
                ast_destroy_node(let_expr);
                KRT_FREE(let_var_name);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            let_clause->data.linq_let.var_name = let_var_name;
            let_clause->data.linq_let.expression = let_expr;
            
            if (clause_count >= capacity) {
                capacity *= 2;
                ASTNode** new_clauses = (ASTNode**)KRT_REALLOC(clauses, capacity * sizeof(ASTNode*));
                if (!new_clauses) {
                    ast_destroy_node(let_clause);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                clauses = new_clauses;
            }
            
            clauses[clause_count++] = let_clause;
        } else if (parser->current_token.type == TOKEN_GROUP) {
            parser_advance(parser);
            
            ASTNode* element_expr = parser_parse_expression(parser);
            if (!element_expr) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            if (parser->current_token.type != TOKEN_BY) {
                ast_destroy_node(element_expr);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            parser_advance(parser);
            
            ASTNode* key_expr = parser_parse_expression(parser);
            if (!key_expr) {
                ast_destroy_node(element_expr);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            char* into_var_name = NULL;
            if (parser->current_token.type == TOKEN_INTO) {
                parser_advance(parser);
                if (parser->current_token.type != TOKEN_IDENTIFIER) {
                    ast_destroy_node(key_expr);
                    ast_destroy_node(element_expr);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                into_var_name = KRT_STRDUP(parser->current_token.value);
                if (!into_var_name) {
                    ast_destroy_node(key_expr);
                    ast_destroy_node(element_expr);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                parser_advance(parser);
            }
            
            ASTNode* group_clause = ast_create_node(AST_LINQ_GROUP, line, col);
            if (!group_clause) {
                if (into_var_name) KRT_FREE(into_var_name);
                ast_destroy_node(key_expr);
                ast_destroy_node(element_expr);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            group_clause->data.linq_group.key_expression = key_expr;
            group_clause->data.linq_group.element_expression = element_expr;
            group_clause->data.linq_group.into_var_name = into_var_name;
            
            if (clause_count >= capacity) {
                capacity *= 2;
                ASTNode** new_clauses = (ASTNode**)KRT_REALLOC(clauses, capacity * sizeof(ASTNode*));
                if (!new_clauses) {
                    ast_destroy_node(group_clause);
                    for (int i = 0; i < clause_count; i++) {
                        ast_destroy_node(clauses[i]);
                    }
                    KRT_FREE(clauses);
                    ast_destroy_node(from_clause);
                    return NULL;
                }
                clauses = new_clauses;
            }
            
            clauses[clause_count++] = group_clause;
            
            if (into_var_name) {
                continue;
            }
            break;
        } else if (parser->current_token.type == TOKEN_SELECT) {
            parser_advance(parser);
            ASTNode* expr = parser_parse_expression(parser);
            if (!expr) {
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            select_clause = ast_create_node(AST_LINQ_SELECT, line, col);
            if (!select_clause) {
                ast_destroy_node(expr);
                for (int i = 0; i < clause_count; i++) {
                    ast_destroy_node(clauses[i]);
                }
                KRT_FREE(clauses);
                ast_destroy_node(from_clause);
                return NULL;
            }
            
            select_clause->data.linq_select.expression = expr;
            select_clause->data.linq_select.key_selector = NULL;
            select_clause->data.linq_select.ascending = true;
            break; 
        } else {
            break;
        }
    }
    
    ASTNode* node = ast_create_node(AST_LINQ_QUERY, line, col);
    if (!node) {
        for (int i = 0; i < clause_count; i++) {
            ast_destroy_node(clauses[i]);
        }
        KRT_FREE(clauses);
        ast_destroy_node(from_clause);
        ast_destroy_node(select_clause);
        return NULL;
    }
    
    node->data.linq_query.from_clause = from_clause;
    node->data.linq_query.clauses = clauses;
    node->data.linq_query.clause_count = clause_count;
    node->data.linq_query.select_clause = select_clause;
    
    return node;
}

// Pattern matching support
static ASTNode* parser_parse_pattern(Parser* parser);

static ASTNode* parser_parse_match_expression(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_MATCH) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser);
    
    ASTNode* expr = parser_parse_expression(parser);
    if (!expr) return NULL;
    
    if (parser->current_token.type != TOKEN_WITH) {
        ast_destroy_node(expr);
        return NULL;
    }
    parser_advance(parser);
    
    int capacity = 8;
    ASTNode** cases = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
    if (!cases) {
        ast_destroy_node(expr);
        return NULL;
    }
    int case_count = 0;
    
    while (parser->current_token.type == TOKEN_PIPE) {
        parser_advance(parser);
        
        ASTNode* pattern = parser_parse_pattern(parser);
        if (!pattern) {
            for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
            KRT_FREE(cases);
            ast_destroy_node(expr);
            return NULL;
        }
        
        ASTNode* when_clause = NULL;
        if (parser->current_token.type == TOKEN_WHEN) {
            parser_advance(parser);
            when_clause = parser_parse_expression(parser);
            if (!when_clause) {
                ast_destroy_node(pattern);
                for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
                KRT_FREE(cases);
                ast_destroy_node(expr);
                return NULL;
            }
        }
        
        if (parser->current_token.type != TOKEN_ARROW) {
            if (when_clause) ast_destroy_node(when_clause);
            ast_destroy_node(pattern);
            for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
            KRT_FREE(cases);
            ast_destroy_node(expr);
            return NULL;
        }
        parser_advance(parser);
        
        ASTNode* body = parser_parse_expression(parser);
        if (!body) {
            if (when_clause) ast_destroy_node(when_clause);
            ast_destroy_node(pattern);
            for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
            KRT_FREE(cases);
            ast_destroy_node(expr);
            return NULL;
        }
        
        ASTNode* case_node = ast_create_node(AST_PATTERN_CASE, line, col);
        if (!case_node) {
            ast_destroy_node(body);
            if (when_clause) ast_destroy_node(when_clause);
            ast_destroy_node(pattern);
            for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
            KRT_FREE(cases);
            ast_destroy_node(expr);
            return NULL;
        }
        
        case_node->data.pattern_case.pattern = pattern;
        case_node->data.pattern_case.when_clause = when_clause;
        case_node->data.pattern_case.body = body;
        
        if (case_count >= capacity) {
            capacity *= 2;
            ASTNode** new_cases = (ASTNode**)KRT_REALLOC(cases, capacity * sizeof(ASTNode*));
            if (!new_cases) {
                ast_destroy_node(case_node);
                for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
                KRT_FREE(cases);
                ast_destroy_node(expr);
                return NULL;
            }
            cases = new_cases;
        }
        cases[case_count++] = case_node;
    }
    
    ASTNode* node = ast_create_node(AST_MATCH_EXPRESSION, line, col);
    if (!node) {
        for (int i = 0; i < case_count; i++) ast_destroy_node(cases[i]);
        KRT_FREE(cases);
        ast_destroy_node(expr);
        return NULL;
    }
    
    node->data.match_expr.expression = expr;
    node->data.match_expr.cases = cases;
    node->data.match_expr.case_count = case_count;
    
    return node;
}

static ASTNode* parser_parse_pattern(Parser* parser) {
    if (!parser) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    if (parser->current_token.type == TOKEN_UNDERSCORE) {
        parser_advance(parser);
        return ast_create_node(AST_PATTERN_WILDCARD, line, col);
    }
    
    if (parser->current_token.type == TOKEN_NUMBER ||
        parser->current_token.type == TOKEN_STRING ||
        parser->current_token.type == TOKEN_TRUE ||
        parser->current_token.type == TOKEN_FALSE ||
        parser->current_token.type == TOKEN_NULL) {
        ASTNode* value = parser_parse_primary(parser);
        if (!value) return NULL;
        
        ASTNode* node = ast_create_node(AST_PATTERN_LITERAL, line, col);
        if (!node) {
            ast_destroy_node(value);
            return NULL;
        }
        node->data.pattern_literal.value = value;
        return node;
    }
    
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);
        
        int capacity = 4;
        ASTNode** elements = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
        if (!elements) return NULL;
        int element_count = 0;
        
        while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            ASTNode* elem = parser_parse_pattern(parser);
            if (!elem) {
                for (int i = 0; i < element_count; i++) ast_destroy_node(elements[i]);
                KRT_FREE(elements);
                return NULL;
            }
            
            if (element_count >= capacity) {
                capacity *= 2;
                ASTNode** new_elements = (ASTNode**)KRT_REALLOC(elements, capacity * sizeof(ASTNode*));
                if (!new_elements) {
                    ast_destroy_node(elem);
                    for (int i = 0; i < element_count; i++) ast_destroy_node(elements[i]);
                    KRT_FREE(elements);
                    return NULL;
                }
                elements = new_elements;
            }
            elements[element_count++] = elem;
            
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                for (int i = 0; i < element_count; i++) ast_destroy_node(elements[i]);
                KRT_FREE(elements);
                return NULL;
            }
        }
        
        parser_advance(parser);
        
        ASTNode* node = ast_create_node(AST_PATTERN_TUPLE, line, col);
        if (!node) {
            for (int i = 0; i < element_count; i++) ast_destroy_node(elements[i]);
            KRT_FREE(elements);
            return NULL;
        }
        node->data.pattern_tuple.elements = elements;
        node->data.pattern_tuple.element_count = element_count;
        return node;
    }
    
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        char* name = KRT_STRDUP(parser->current_token.value);
        if (!name) return NULL;
        parser_advance(parser);
        
        ASTNode* node = ast_create_node(AST_PATTERN_VARIABLE, line, col);
        if (!node) {
            KRT_FREE(name);
            return NULL;
        }
        node->data.pattern_variable.name = name;
        return node;
    }
    
    return NULL;
}

static ASTNode* parser_parse_fixed_statement(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_FIXED) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser); // consume 'fixed'
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser); // consume '('
    
    // Parse variable declaration: ptr = &obj
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    char* var_name = KRT_STRDUP(parser->current_token.value);
    if (!var_name) {
        return NULL;
    }
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_ASSIGN) {
        KRT_FREE(var_name);
        return NULL;
    }
    parser_advance(parser); // consume '='
    
    ASTNode* expr = parser_parse_expression(parser);
    if (!expr) {
        KRT_FREE(var_name);
        return NULL;
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(expr);
        KRT_FREE(var_name);
        return NULL;
    }
    parser_advance(parser); // consume ')'
    
    // Parse body
    ASTNode* body = parser_parse_statement(parser);
    if (!body) {
        ast_destroy_node(expr);
        KRT_FREE(var_name);
        return NULL;
    }
    
    ASTNode* node = ast_create_node(AST_FIXED_STATEMENT, line, col);
    if (!node) {
        ast_destroy_node(body);
        ast_destroy_node(expr);
        KRT_FREE(var_name);
        return NULL;
    }

    node->data.fixed_statement.variable_name = var_name;
    node->data.fixed_statement.expression = expr;
    node->data.fixed_statement.body = body;

    return node;
}

static ASTNode* parser_parse_generic_constraints(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_WHERE) return NULL;

    int line = parser->current_token.line;
    int col = parser->current_token.column;

    ASTNode* constraint_list = ast_create_node(AST_GENERIC_CONSTRAINT, line, col);
    if (!constraint_list) return NULL;

    int capacity = 4;
    ASTNode** constraints = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
    if (!constraints) {
        ast_destroy_node(constraint_list);
        return NULL;
    }

    int count = 0;

    while (parser->current_token.type == TOKEN_WHERE) {
        parser_advance(parser); // consume 'where'

        // Expect type parameter name
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            for (int i = 0; i < count; i++) {
                ast_destroy_node(constraints[i]);
            }
            KRT_FREE(constraints);
            ast_destroy_node(constraint_list);
            return NULL;
        }

        char* param_name = KRT_STRDUP(parser->current_token.value);
        parser_advance(parser);

        // Expect ':'
        if (parser->current_token.type != TOKEN_COLON) {
            KRT_FREE(param_name);
            for (int i = 0; i < count; i++) {
                ast_destroy_node(constraints[i]);
            }
            KRT_FREE(constraints);
            ast_destroy_node(constraint_list);
            return NULL;
        }
        parser_advance(parser); // consume ':'

        // Parse constraint type
        char* constraint_type = NULL;
        ASTNode* interface_constraint = NULL;

        if (parser->current_token.type == TOKEN_CLASS) {
            constraint_type = KRT_STRDUP("class");
            parser_advance(parser);
        } else if (parser->current_token.type == TOKEN_STRUCT) {
            constraint_type = KRT_STRDUP("struct");
            parser_advance(parser);
        } else if (parser->current_token.type == TOKEN_NEW) {
            // new() constraint
            constraint_type = KRT_STRDUP("new");
            parser_advance(parser);
            if (parser->current_token.type == TOKEN_LEFT_PAREN) {
                parser_advance(parser); // consume '('
                if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                    parser_advance(parser); // consume ')'
                }
            }
        } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
            // Interface or base class constraint
            constraint_type = KRT_STRDUP("interface");
            interface_constraint = ast_create_node(AST_IDENTIFIER, parser->current_token.line, parser->current_token.column);
            if (interface_constraint) {
                interface_constraint->data.identifier_name = KRT_STRDUP(parser->current_token.value);
            }
            parser_advance(parser);
        } else {
            KRT_FREE(param_name);
            for (int i = 0; i < count; i++) {
                ast_destroy_node(constraints[i]);
            }
            KRT_FREE(constraints);
            ast_destroy_node(constraint_list);
            return NULL;
        }

        if (count >= capacity) {
            capacity *= 2;
            ASTNode** new_constraints = (ASTNode**)KRT_REALLOC(constraints, capacity * sizeof(ASTNode*));
            if (!new_constraints) {
                KRT_FREE(param_name);
                KRT_FREE(constraint_type);
                if (interface_constraint) ast_destroy_node(interface_constraint);
                for (int i = 0; i < count; i++) {
                    ast_destroy_node(constraints[i]);
                }
                KRT_FREE(constraints);
                ast_destroy_node(constraint_list);
                return NULL;
            }
            constraints = new_constraints;
        }

        ASTNode* constraint = ast_create_node(AST_GENERIC_CONSTRAINT, parser->current_token.line, parser->current_token.column);
        if (!constraint) {
            KRT_FREE(param_name);
            KRT_FREE(constraint_type);
            if (interface_constraint) ast_destroy_node(interface_constraint);
            for (int i = 0; i < count; i++) {
                ast_destroy_node(constraints[i]);
            }
            KRT_FREE(constraints);
            ast_destroy_node(constraint_list);
            return NULL;
        }

        constraint->data.generic_constraint.param_name = param_name;
        constraint->data.generic_constraint.constraint_type = constraint_type;
        constraint->data.generic_constraint.interface_constraint = interface_constraint;

        constraints[count++] = constraint;

        // Check for more where clauses
        if (parser->current_token.type != TOKEN_WHERE) {
            break;
        }
    }

    // Store constraints in the list node
    // Note: This is a simplified approach - in a real implementation,
    // you might want to create a proper list node type
    constraint_list->data.generic_constraint.param_name = NULL;
    constraint_list->data.generic_constraint.constraint_type = NULL;
    constraint_list->data.generic_constraint.interface_constraint = (ASTNode*)constraints;

    return constraint_list;
}

static ASTNode* parser_parse_stackalloc_expression(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_STACKALLOC) return NULL;

    int line = parser->current_token.line;
    int col = parser->current_token.column;

    parser_advance(parser); // consume 'stackalloc'

    // Parse element type
    if (!parser_is_type_keyword(parser->current_token.type)) {
        return NULL;
    }
    KrtTokenType element_type = parser->current_token.type;
    parser_advance(parser);

    // Expect '['
    if (parser->current_token.type != TOKEN_LEFT_BRACKET) {
        return NULL;
    }
    parser_advance(parser); // consume '['

    // Parse size expression
    ASTNode* size_expr = parser_parse_expression(parser);
    if (!size_expr) {
        return NULL;
    }

    // Expect ']'
    if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
        ast_destroy_node(size_expr);
        return NULL;
    }
    parser_advance(parser); // consume ']'

    ASTNode* node = ast_create_node(AST_STACKALLOC_EXPRESSION, line, col);
    if (!node) {
        ast_destroy_node(size_expr);
        return NULL;
    }

    node->data.stackalloc_expr.type_token = element_type;
    node->data.stackalloc_expr.count_expr = size_expr;

    return node;
}

static ASTNode* parser_parse_await_expression(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_AWAIT) return NULL;

    int line = parser->current_token.line;
    int col = parser->current_token.column;

    parser_advance(parser); // consume 'await'

    ASTNode* expr = parser_parse_expression(parser);
    if (!expr) {
        return NULL;
    }

    ASTNode* node = ast_create_node(AST_AWAIT_EXPRESSION, line, col);
    if (!node) {
        ast_destroy_node(expr);
        return NULL;
    }

    node->data.await_expr.expression = expr;

    return node;
}