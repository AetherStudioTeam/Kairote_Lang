
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
static ASTNode* parser_parse_constructor_declaration(Parser* parser);
static ASTNode* parser_parse_destructor_declaration(Parser* parser);
static ASTNode* parser_parse_static_member_declaration(Parser* parser);
static ASTNode* parser_parse_static_variable_declaration(Parser* parser);
static ASTNode* parser_parse_static_function_declaration(Parser* parser);
static ASTNode* parser_parse_function_declaration(Parser* parser);
static ASTNode* parser_parse_ternary_operation(Parser* parser);
static ASTNode* parser_parse_type_declaration(Parser* parser);
static ASTNode* parser_parse_typed_function_declaration(Parser* parser, KrtTokenType return_type, char* function_name, int is_static, int line, int col);
static int parser_is_type_keyword(KrtTokenType type);
static int parser_parse_parameter_list(Parser* parser, char*** parameters, KrtTokenType** parameter_types, int* parameter_count);
static void parser_free_parameter_list(char** parameters, KrtTokenType* parameter_types, int parameter_count);
static ASTNode* parser_create_function_node(Parser* parser, int is_static, char* name, char** parameters, KrtTokenType* parameter_types, int parameter_count, ASTNode* body, KrtTokenType return_type, int line, int col);
static ASTNode* parser_parse_property_declaration(Parser* parser, KrtTokenType type, char* name, ASTNode** attributes, int attribute_count);
static ASTNode* parser_parse_lambda_expression(Parser* parser, char** parameters, int parameter_count);
static ASTNode* parser_parse_linq_query(Parser* parser);
static ASTNode* parser_parse_attribute(Parser* parser);
static ASTNode* parser_parse_attributes(Parser* parser);
static int parser_parse_lambda_parameters(Parser* parser, char*** parameters, int* parameter_count);
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

static int parser_is_declared_function(Parser* parser, const char* identifier) {
    if (!parser || !identifier) {
        return 0;
    }
    for (int i = 0; i < parser->declared_function_count; i++) {
        if (strcmp(parser->declared_functions[i], identifier) == 0) {
            return 1;
        }
    }
    return 0;
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
}

static int parser_parse_parameter_list(Parser* parser, char*** parameters, KrtTokenType** parameter_types, int* parameter_count) {
    if (!parameters || !parameter_types || !parameter_count) {
        return 0;
    }
    *parameters = NULL;
    *parameter_types = NULL;
    *parameter_count = 0;
    if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
        return 1;
    }
    while (1) {
        KrtTokenType param_type = TOKEN_UNKNOWN;
        if (parser_is_type_keyword(parser->current_token.type)) {
            param_type = parser->current_token.type;
            parser_advance(parser);
        }
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }
        char* param_name = KRT_STRDUP(parser->current_token.value);
        if (!param_name) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }
        char** new_parameters = (char**)KRT_REALLOC(*parameters, (*parameter_count + 1) * sizeof(char*));
        KrtTokenType* new_param_types = (KrtTokenType*)KRT_REALLOC(*parameter_types, (*parameter_count + 1) * sizeof(KrtTokenType));
        if (!new_parameters || !new_param_types) {
            KRT_FREE(param_name);
            if (new_parameters) {
                *parameters = new_parameters;
            }
            if (new_param_types) {
                *parameter_types = new_param_types;
            }
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            *parameters = NULL;
            *parameter_types = NULL;
            *parameter_count = 0;
            return 0;
        }
        *parameters = new_parameters;
        *parameter_types = new_param_types;
        (*parameters)[*parameter_count] = param_name;
        (*parameter_types)[*parameter_count] = param_type;
        (*parameter_count)++;
        parser_advance(parser);
        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type)) {
                parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
                return 0;
            }
            (*parameter_types)[*parameter_count - 1] = parser->current_token.type;
            parser_advance(parser);
        }
        if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
            break;
        }
        if (parser->current_token.type != TOKEN_COMMA) {
            parser_free_parameter_list(*parameters, *parameter_types, *parameter_count);
            return 0;
        }
        parser_advance(parser);
    }
    return 1;
}

static ASTNode* parser_create_function_node(Parser* parser,
                                            int is_static,
                                            char* name,
                                            char** parameters,
                                            KrtTokenType* parameter_types,
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
        node->data.static_function_decl.parameter_count = parameter_count;
        node->data.static_function_decl.body = body;
        node->data.static_function_decl.return_type = return_type;
    } else {
        node->data.function_decl.name = name;
        node->data.function_decl.parameters = parameters;
        node->data.function_decl.parameter_types = parameter_types;
        node->data.function_decl.parameter_count = parameter_count;
        node->data.function_decl.body = body;
        node->data.function_decl.return_type = return_type;
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
    int col = parser->current_token.column;
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
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);
    if (parser->current_token.type == TOKEN_COLON) {
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* body = parser_parse_block(parser);
    if (!body) {
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_NAMESPACE_DECLARATION, line, col);
    node->data.namespace_decl.name = name;
    node->data.namespace_decl.body = body;
    return node;
}

static ASTNode* parser_parse_using_directive(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser);
    
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

static ASTNode* parser_parse_class_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    parser_advance(parser);
    
    char* old_class = parser->current_class;
    parser->current_class = name;
    
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
        KRT_FREE(name);
        if (base_class) {
            ast_destroy_node(base_class);
        }
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_CLASS_DECLARATION, line, col);
    node->data.class_decl.name = name;
    node->data.class_decl.body = body;
    node->data.class_decl.base_class = base_class;
    return node;
}

static ASTNode* parser_parse_primary(Parser* parser) {
    ASTNode* node = NULL;
    int line = parser->current_token.line;
    int col = parser->current_token.column;
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
                
                parser_advance(parser); 
                
                char** params = NULL;
                int param_count = 0;
                
                if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                    
                    parser_advance(parser); 
                } else if (parser->current_token.type == TOKEN_IDENTIFIER) {
                    
                    int capacity = 4;
                    params = (char**)KRT_MALLOC(capacity * sizeof(char*));
                    if (!params) {
                        return NULL;
                    }
                    
                    while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                        if (parser->current_token.type != TOKEN_IDENTIFIER) {
                            for (int i = 0; i < param_count; i++) {
                                KRT_FREE(params[i]);
                            }
                            KRT_FREE(params);
                            return NULL;
                        }
                        
                        if (param_count >= capacity) {
                            capacity *= 2;
                            char** new_params = (char**)KRT_REALLOC(params, capacity * sizeof(char*));
                            if (!new_params) {
                                for (int i = 0; i < param_count; i++) {
                                    KRT_FREE(params[i]);
                                }
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
                            for (int i = 0; i < param_count; i++) {
                                KRT_FREE(params[i]);
                            }
                            KRT_FREE(params);
                            return NULL;
                        }
                    }
                    
                    parser_advance(parser); 
                } else {
                    
                    ASTNode* expr = parser_parse_expression(parser);
                    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                        ast_destroy_node(expr);
                        return NULL;
                    }
                    parser_advance(parser);
                    return expr;
                }
                
                if (parser->current_token.type == TOKEN_LAMBDA) {
                    return parser_parse_lambda_expression(parser, params, param_count);
                }
                
                for (int i = 0; i < param_count; i++) {
                    KRT_FREE(params[i]);
                }
                KRT_FREE(params);
                
                return NULL;
            }
            token_free(&peek1);
            
            parser_advance(parser);
            if (parser_is_type_keyword(parser->current_token.type)) {
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
        case TOKEN_TEMPLATE: return NULL;
        
        case TOKEN_FROM:
            return parser_parse_linq_query(parser);
        default: return NULL;
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
    ASTNode* left = parser_parse_primary(parser);
    if (!left) {
        return NULL;
    }
    while (parser->current_token.type == TOKEN_DOT ||
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
        } else if (parser->current_token.type == TOKEN_DOT) {
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
    return condition;
}

static ASTNode* parser_parse_expression(Parser* parser) {
    return parser_parse_ternary_operation(parser);
}

static ASTNode* parser_parse_variable_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    int is_let = (parser->current_token.type == TOKEN_LET) ? 1 : 0;
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    char* name = KRT_STRDUP(parser->current_token.value);
    if (!name) {
        return NULL;
    }
    parser_advance(parser);
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
    ASTNode* value = NULL;
    if (parser->current_token.type == TOKEN_ASSIGN) {
        parser_advance(parser);
        value = parser_parse_expression(parser);
        if (!value) {
            KRT_FREE(name);
            if (array_size) {
                ast_destroy_node(array_size);
            }
            return NULL;
        }
    }
    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION, line, col);
    if (!node) {
        KRT_FREE(name);
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
    node->data.variable_decl.array_size = array_size;
    node->data.variable_decl.is_array = is_array;
    node->data.variable_decl.is_let = is_let;
    return node;
}

static ASTNode* parser_parse_static_member_declaration(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_STATIC) {
        return NULL;
    }
    int line = parser->current_token.line;
    int col = parser->current_token.column;
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
        int parameter_count = 0;
        if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
            KRT_FREE(name);
            return NULL;
        }
        if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
        parser_advance(parser);
        
        if (parser->current_token.type == TOKEN_COLON) {
            parser_advance(parser);
            if (!parser_is_type_keyword(parser->current_token.type)) {
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
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
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
                KRT_FREE(name);
                return NULL;
            }
        } else if (parser->current_token.type == TOKEN_SEMICOLON) {
            parser_advance(parser);
        } else {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
        ASTNode* node = parser_create_function_node(parser, 1, name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type_token,
                                                func_line, func_col);
        if (!node) {
            if (body) {
                ast_destroy_node(body);
            }
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
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

static ASTNode* parser_parse_static_variable_declaration(Parser* parser) {
    ASTNode* node = parser_parse_static_member_declaration(parser);
    if (!node) {
        return NULL;
    }
    if (node->type != AST_STATIC_VARIABLE_DECLARATION) {
        ast_destroy_node(node);
        return NULL;
    }
    return node;
}

static ASTNode* parser_parse_static_function_declaration(Parser* parser) {
    ASTNode* node = parser_parse_static_member_declaration(parser);
    if (!node) {
        return NULL;
    }
    if (node->type != AST_STATIC_FUNCTION_DECLARATION) {
        ast_destroy_node(node);
        return NULL;
    }
    return node;
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
    if (parser->current_token.type == TOKEN_STATIC) {
        ASTNode* member = parser_parse_static_member_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    } else if (parser->current_class && 
               parser->current_token.type == TOKEN_IDENTIFIER &&
               strcmp(parser->current_token.value, parser->current_class) == 0) {
        ASTNode* member = parser_parse_constructor_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
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
            ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
            if (!node) {
                ast_destroy_node(member);
                return NULL;
            }
            node->data.access_modifier.access_modifier = access_type;
            node->data.access_modifier.member = member;
            return node;
        }
        token_free(&peek);
        return NULL;
    } else if (parser_is_type_keyword(parser->current_token.type)) {
        ASTNode* member = parser_parse_type_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    } else if (parser->current_token.type == TOKEN_FUNCTION) {
        ASTNode* member = parser_parse_function_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    } else if (parser->current_token.type == TOKEN_VAR || parser->current_token.type == TOKEN_LET) {
        ASTNode* member = parser_parse_variable_declaration(parser);
        if (!member) {
            return NULL;
        }
        ASTNode* node = ast_create_node(AST_ACCESS_MODIFIER, line, col);
        if (!node) {
            ast_destroy_node(member);
            return NULL;
        }
        node->data.access_modifier.access_modifier = access_type;
        node->data.access_modifier.member = member;
        return node;
    }
    return NULL;
}

static ASTNode* parser_parse_constructor_declaration(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    char** parameters = NULL;
    KrtTokenType* parameter_types = NULL;
    int parameter_count = 0;
    if (parser->current_token.type != TOKEN_LEFT_PAREN) {
        return NULL;
    }
    parser_advance(parser);
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        while (1) {
            KrtTokenType param_type = TOKEN_UNKNOWN;
            if ((parser->current_token.type >= TOKEN_INT8 && parser->current_token.type <= TOKEN_VOID) ||
                parser->current_token.type == TOKEN_STRING) {
                param_type = parser->current_token.type;
                parser_advance(parser);
            }
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
                return NULL;
            }
            char** new_parameters = KRT_REALLOC(parameters, (parameter_count + 1) * sizeof(char*));
            KrtTokenType* new_parameter_types = KRT_REALLOC(parameter_types, (parameter_count + 1) * sizeof(KrtTokenType));
            if (!new_parameters || !new_parameter_types) {
                KRT_FREE(parser->current_token.value);
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
                return NULL;
            }
            parameters = new_parameters;
            parameter_types = new_parameter_types;
            parameters[parameter_count] = KRT_STRDUP(parser->current_token.value);
            parameter_types[parameter_count] = param_type;
            parameter_count++;
            parser_advance(parser);
            if (parser->current_token.type == TOKEN_RIGHT_PAREN) {
                break;
            }
            if (parser->current_token.type != TOKEN_COMMA) {
                parser_free_parameter_list(parameters, parameter_types, parameter_count);
                return NULL;
            }
            parser_advance(parser);
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        return NULL;
    }
    ASTNode* node = ast_create_node(AST_CONSTRUCTOR_DECLARATION, line, col);
    if (!node) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        if (body) {
            ast_destroy_node(body);
        }
        return NULL;
    }
    node->data.constructor_decl.parameters = parameters;
    node->data.constructor_decl.parameter_types = parameter_types;
    node->data.constructor_decl.parameter_count = parameter_count;
    node->data.constructor_decl.body = body;
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
    int parameter_count = 0;
    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
        KRT_FREE(name);
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            KRT_FREE(name);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        KRT_FREE(name);
        return NULL;
    }
    ASTNode* node = parser_create_function_node(parser, 0, name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type,
                                                line, col);
    if (!node) {
        if (body) {
            ast_destroy_node(body);
        }
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        KRT_FREE(name);
        return NULL;
    }
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
    int parameter_count = 0;
    if (!parser_parse_parameter_list(parser, &parameters, &parameter_types, &parameter_count)) {
        KRT_FREE(function_name);
        return NULL;
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        KRT_FREE(function_name);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LEFT_BRACE) {
        body = parser_parse_block(parser);
        if (!body) {
            parser_free_parameter_list(parameters, parameter_types, parameter_count);
            KRT_FREE(function_name);
            return NULL;
        }
    } else if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    } else {
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
        KRT_FREE(function_name);
        return NULL;
    }
    ASTNode* node = parser_create_function_node(parser,
                                                is_static,
                                                function_name,
                                                parameters,
                                                parameter_types,
                                                parameter_count,
                                                body,
                                                return_type,
                                                line, col);
    if (!node) {
        if (body) {
            ast_destroy_node(body);
        }
        parser_free_parameter_list(parameters, parameter_types, parameter_count);
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
    if (parser->current_token.type != TOKEN_LEFT_BRACE) {
        return parser_parse_statement(parser);
    }
    parser_advance(parser);
    ASTNode** statements = NULL;
    int statement_count = 0;
    while (parser->current_token.type != TOKEN_RIGHT_BRACE &&
           parser->current_token.type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (!stmt) {
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
        for (int i = 0; i < statement_count; i++) {
            ast_destroy_node(statements[i]);
        }
        KRT_FREE(statements);
        return NULL;
    }
    parser_advance(parser);
    ASTNode* node = ast_create_node(AST_BLOCK, line, col);
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
    int line = parser->current_token.line;
    int col = parser->current_token.column;
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
        case TOKEN_CLASS:
            node = parser_parse_class_declaration(parser);
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
    ASTNode* program = ast_create_node(AST_PROGRAM, line, col);
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

ASTNode* parser_parse_throw_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    ASTNode* exception_expr = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON && parser->current_token.type != TOKEN_EOF) {
        exception_expr = parser_parse_expression(parser);
        if (!exception_expr) {
            return NULL;
        }
    }
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    ASTNode* node = ast_create_node(AST_THROW_STATEMENT, line, col);
    if (!node) {
        if (exception_expr) {
            ast_destroy_node(exception_expr);
        }
        return NULL;
    }
    node->data.throw_stmt.exception_expr = exception_expr;
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
    const char* type_str = token_type_to_string(type_token);
    char* type_name = KRT_STRDUP(type_str ? type_str : "UNKNOWN");
    parser_advance(parser);
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
    node->data.variable_decl.is_array = false;
    node->data.variable_decl.template_instantiation_type = NULL;
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
    
    bool has_var = false;
    if (parser->current_token.type == TOKEN_VAR || parser->current_token.type == TOKEN_LET) {
        has_var = true;
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
static ASTNode* parser_parse_attribute(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_LEFT_BRACKET) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    char* name = KRT_STRDUP(parser->current_token.value);
    if (!name) return NULL;
    parser_advance(parser);
    
    ASTNode** arguments = NULL;
    int argument_count = 0;
    int capacity = 0;
    
    if (parser->current_token.type == TOKEN_LEFT_PAREN) {
        parser_advance(parser);
        
        capacity = PARSER_PARAMETER_CAPACITY_INIT;
        arguments = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
        if (!arguments) {
            KRT_FREE(name);
            return NULL;
        }
        
        while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            ASTNode* arg = parser_parse_expression(parser);
            if (!arg) {
                PARSER_FREE_STRING_ARRAY(arguments, argument_count);
                KRT_FREE(name);
                return NULL;
            }
            
            if (argument_count >= capacity) {
                capacity *= 2;
                ASTNode** new_args = (ASTNode**)KRT_REALLOC(arguments, capacity * sizeof(ASTNode*));
                if (!new_args) {
                    ast_destroy_node(arg);
                    PARSER_FREE_STRING_ARRAY(arguments, argument_count);
                    KRT_FREE(name);
                    return NULL;
                }
                arguments = new_args;
            }
            
            arguments[argument_count++] = arg;
            
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
                ast_destroy_node(arg);
                PARSER_FREE_STRING_ARRAY(arguments, argument_count);
                KRT_FREE(name);
                return NULL;
            }
        }
        
        parser_advance(parser); 
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
        PARSER_FREE_STRING_ARRAY(arguments, argument_count);
        KRT_FREE(name);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_ATTRIBUTE, line, col);
    if (!node) {
        PARSER_FREE_STRING_ARRAY(arguments, argument_count);
        KRT_FREE(name);
        return NULL;
    }
    
    node->data.attribute.name = name;
    node->data.attribute.arguments = arguments;
    node->data.attribute.argument_count = argument_count;
    node->data.attribute.named_arguments = NULL;
    
    return node;
}
static ASTNode* parser_parse_attributes(Parser* parser) {
    if (!parser || parser->current_token.type != TOKEN_LEFT_BRACKET) return NULL;
    
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    int capacity = PARSER_ATTRIBUTE_CAPACITY_INIT;
    int count = 0;
    ASTNode** attributes = (ASTNode**)KRT_MALLOC(capacity * sizeof(ASTNode*));
    if (!attributes) return NULL;
    
    while (parser->current_token.type == TOKEN_LEFT_BRACKET) {
        ASTNode* attr = parser_parse_attribute(parser);
        if (!attr) {
            PARSER_FREE_STRING_ARRAY(attributes, count);
            return NULL;
        }
        
        if (count >= capacity) {
            capacity *= 2;
            ASTNode** new_attrs = (ASTNode**)KRT_REALLOC(attributes, capacity * sizeof(ASTNode*));
            if (!new_attrs) {
                ast_destroy_node(attr);
                PARSER_FREE_STRING_ARRAY(attributes, count);
                return NULL;
            }
            attributes = new_attrs;
        }
        
        attributes[count++] = attr;
    }
    
    ASTNode* node = ast_create_node(AST_ATTRIBUTE_LIST, line, col);
    if (!node) {
        PARSER_FREE_STRING_ARRAY(attributes, count);
        return NULL;
    }
    
    node->data.attribute_list.attributes = attributes;
    node->data.attribute_list.attribute_count = count;
    node->data.attribute_list.target = NULL;
    
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