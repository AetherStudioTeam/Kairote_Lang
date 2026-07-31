#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Accelerator.h"
#include "Core/Utils/KrtCommon.h"
#include "Core/Memory/Arena.h"
#include "Parser.h"
#include "ParserBase.h"
#include "ParserExpression.h"
#include "../Lexer/Tokenizer.h"

ASTNode* parser_parse_statement(Parser* parser);
ASTNode* parser_parse_block(Parser* parser);

#define PARSER_LIKELY(x) (x)
#define PARSER_UNLIKELY(x) (x)
#define PARSER_ARGUMENT_CAPACITY_INIT 8

#define PARSER_MALLOC(size) KRT_MALLOC(size)
#define PARSER_REALLOC(ptr, size) KRT_REALLOC(ptr, size)
#define PARSER_FREE(ptr) KRT_FREE(ptr)

#define PARSER_ALLOC_FROM_ARENA(parser, size) KrtArenaAlloc((parser)->arena, size)

#define PARSER_CREATE_NODE(type, line, col) ast_create_node_arena(type, line, col, parser->arena)
#define PARSER_STRDUP(s) arena_strdup(parser->arena, s)

#undef ast_create_node
#define ast_create_node(type, line, col) ast_create_node_arena(type, line, col, parser->arena)

static char* arena_strdup(KrtArena* arena, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* result = (char*)KrtArenaAlloc(arena, len);
    if (result) memcpy(result, str, len);
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

    if (!name) return NULL;

    ASTNode* array_size = NULL;
    bool is_array = false;

    if (parser->current_token.type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);
        is_array = true;
        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            array_size = parser_parse_expression(parser);
            if (!array_size) { KRT_FREE(name); return NULL; }
        }
        if (parser->current_token.type != TOKEN_RIGHT_BRACKET) {
            KRT_FREE(name);
            if (array_size) ast_destroy_node(array_size);
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
            if (!type_name) { KRT_FREE(name); if (array_size) ast_destroy_node(array_size); return NULL; }
            parser_advance(parser);
        } else {
            KRT_FREE(name);
            if (array_size) ast_destroy_node(array_size);
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
            if (array_size) ast_destroy_node(array_size);
            return NULL;
        }
    }

    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION, line, col);
    if (!node) {
        KRT_FREE(name);
        KRT_FREE(type_name);
        if (value) ast_destroy_node(value);
        if (array_size) ast_destroy_node(array_size);
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

static ASTNode* parser_parse_assignment_from_left(Parser* parser, ASTNode* left) {
    if (!left) return NULL;

    KrtTokenType operator = parser->current_token.type;
    if (operator != TOKEN_ASSIGN && operator != TOKEN_PLUS_ASSIGN &&
        operator != TOKEN_MINUS_ASSIGN && operator != TOKEN_MUL_ASSIGN &&
        operator != TOKEN_DIV_ASSIGN && operator != TOKEN_MOD_ASSIGN) {
        return left;
    }

    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);

    ASTNode* value = parser_parse_expression(parser);
    if (!value) { ast_destroy_node(left); return NULL; }

    if (left->type == AST_ARRAY_ACCESS) {
        ASTNode* node = (operator == TOKEN_ASSIGN) ?
            ast_create_node(AST_ARRAY_ASSIGNMENT, line, col) :
            ast_create_node(AST_ARRAY_COMPOUND_ASSIGNMENT, line, col);
        if (!node) { ast_destroy_node(left); ast_destroy_node(value); return NULL; }

        if (operator == TOKEN_ASSIGN) {
            node->data.array_assignment.array = left->data.array_access.array;
            node->data.array_assignment.index = left->data.array_access.index;
            node->data.array_assignment.value = value;
        } else {
            node->data.array_compound_assignment.array = left->data.array_access.array;
            node->data.array_compound_assignment.index = left->data.array_access.index;
            node->data.array_compound_assignment.value = value;
            node->data.array_compound_assignment.operator = operator;
        }
        KRT_FREE(left);
        return node;
    } else if (left->type == AST_IDENTIFIER) {
        ASTNode* node = (operator == TOKEN_ASSIGN) ?
            ast_create_node(AST_ASSIGNMENT, line, col) :
            ast_create_node(AST_COMPOUND_ASSIGNMENT, line, col);
        if (!node) { ast_destroy_node(left); ast_destroy_node(value); return NULL; }

        if (operator == TOKEN_ASSIGN) {
            node->data.assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.assignment.value = value;
        } else {
            node->data.compound_assignment.name = KRT_STRDUP(left->data.identifier_name);
            node->data.compound_assignment.value = value;
            node->data.compound_assignment.operator = operator;
        }
        ast_destroy_node(left);
        return node;
    } else if (left->type == AST_MEMBER_ACCESS) {
        if (operator == TOKEN_ASSIGN) {
            ASTNode* node = ast_create_node(AST_BINARY_OPERATION, line, col);
            if (!node) { ast_destroy_node(left); ast_destroy_node(value); return NULL; }
            node->data.binary_op.left = left;
            node->data.binary_op.operator = operator;
            node->data.binary_op.right = value;
            return node;
        }
    }

    ast_destroy_node(left);
    ast_destroy_node(value);
    return NULL;
}

ASTNode* parser_parse_return_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    ASTNode* value = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON && 
        parser->current_token.type != TOKEN_RIGHT_BRACE &&
        parser->current_token.type != TOKEN_EOF) {
        value = parser_parse_expression(parser);
    }
    
    ASTNode* node = ast_create_node(AST_RETURN_STATEMENT, line, col);
    if (!node) { if (value) ast_destroy_node(value); return NULL; }
    node->data.return_stmt.value = value;
    return node;
}

ASTNode* parser_parse_print_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) { return NULL; }
    parser_advance(parser);
    
    ASTNode** values = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, sizeof(ASTNode*) * PARSER_ARGUMENT_CAPACITY_INIT);
    int count = 0, capacity = PARSER_ARGUMENT_CAPACITY_INIT;
    
    while (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        if (count >= capacity) {
            capacity *= 2;
            values = (ASTNode**)KRT_REALLOC(values, capacity * sizeof(ASTNode*));
            if (!values) return NULL;
        }
        
        values[count++] = parser_parse_expression(parser);
        if (!values[count - 1]) break;
        
        if (parser->current_token.type == TOKEN_COMMA) {
            parser_advance(parser);
        } else if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
            break;
        }
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        for (int i = 0; i < count; i++) ast_destroy_node(values[i]);
        KRT_FREE(values);
        return NULL;
    }
    parser_advance(parser);
    
    bool has_newline = true;
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    
    ASTNode* node = ast_create_node(AST_PRINT_STATEMENT, line, col);
    if (!node) { 
        for (int i = 0; i < count; i++) ast_destroy_node(values[i]); 
        KRT_FREE(values); 
        return NULL; 
    }
    node->data.print_stmt.values = values;
    node->data.print_stmt.value_count = count;
    node->data.print_stmt.has_newline = has_newline;
    return node;
}

ASTNode* parser_parse_if_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) return NULL;
    parser_advance(parser);
    
    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) return NULL;
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* then_branch = parser_parse_block(parser);
    if (!then_branch) { ast_destroy_node(condition); return NULL; }
    
    ASTNode* else_branch = NULL;
    if (parser->current_token.type == TOKEN_ELSE) {
        parser_advance(parser);
        if (parser->current_token.type == TOKEN_IF) {
            else_branch = parser_parse_if_statement(parser);
        } else {
            else_branch = parser_parse_block(parser);
        }
        if (!else_branch) { ast_destroy_node(condition); ast_destroy_node(then_branch); return NULL; }
    }
    
    ASTNode* node = ast_create_node(AST_IF_STATEMENT, line, col);
    if (!node) { 
        ast_destroy_node(condition); 
        ast_destroy_node(then_branch); 
        if (else_branch) ast_destroy_node(else_branch); 
        return NULL; 
    }
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    return node;
}

ASTNode* parser_parse_while_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) return NULL;
    parser_advance(parser);
    
    ASTNode* condition = parser_parse_expression(parser);
    if (!condition) return NULL;
    
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* body = parser_parse_block(parser);
    if (!body) { ast_destroy_node(condition); return NULL; }
    
    ASTNode* node = ast_create_node(AST_WHILE_STATEMENT, line, col);
    if (!node) { ast_destroy_node(condition); ast_destroy_node(body); return NULL; }
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

ASTNode* parser_parse_for_statement(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    parser_advance(parser);
    
    if (parser->current_token.type != TOKEN_LEFT_PAREN) return NULL;
    parser_advance(parser);
    
    ASTNode* init = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        init = parser_parse_statement(parser);
        if (!init) return NULL;
    }
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (init) ast_destroy_node(init);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* condition = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        condition = parser_parse_expression(parser);
        if (!condition) { if (init) ast_destroy_node(init); return NULL; }
    }
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        if (init) ast_destroy_node(init);
        if (condition) ast_destroy_node(condition);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* increment = NULL;
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        increment = parser_parse_expression(parser);
        if (!increment) { 
            if (init) ast_destroy_node(init); 
            if (condition) ast_destroy_node(condition); 
            return NULL; 
        }
    }
    if (parser->current_token.type != TOKEN_RIGHT_PAREN) {
        if (init) ast_destroy_node(init);
        if (condition) ast_destroy_node(condition);
        if (increment) ast_destroy_node(increment);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* body = parser_parse_block(parser);
    if (!body) { 
        if (init) ast_destroy_node(init); 
        if (condition) ast_destroy_node(condition); 
        if (increment) ast_destroy_node(increment); 
        return NULL; 
    }
    
    ASTNode* node = ast_create_node(AST_FOR_STATEMENT, line, col);
    if (!node) { 
        if (init) ast_destroy_node(init); 
        if (condition) ast_destroy_node(condition); 
        if (increment) ast_destroy_node(increment); 
        ast_destroy_node(body); 
        return NULL; 
    }
    node->data.for_stmt.init = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.increment = increment;
    node->data.for_stmt.body = body;
    return node;
}

ASTNode* parser_parse_block(Parser* parser) {
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    
    if (parser->current_token.type != TOKEN_LEFT_BRACE) return NULL;
    parser_advance(parser);
    
    ASTNode** statements = (ASTNode**)PARSER_ALLOC_FROM_ARENA(parser, sizeof(ASTNode*) * 16);
    int count = 0, capacity = 16;
    
    while (parser->current_token.type != TOKEN_RIGHT_BRACE && 
           parser->current_token.type != TOKEN_EOF) {
        if (count >= capacity) {
            capacity *= 2;
            statements = (ASTNode**)KRT_REALLOC(statements, capacity * sizeof(ASTNode*));
            if (!statements) return NULL;
        }
        
        statements[count++] = parser_parse_statement(parser);
        if (!statements[count - 1]) break;
    }
    
    if (parser->current_token.type != TOKEN_RIGHT_BRACE) {
        for (int i = 0; i < count; i++) ast_destroy_node(statements[i]);
        KRT_FREE(statements);
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = ast_create_node(AST_BLOCK, line, col);
    if (!node) { 
        for (int i = 0; i < count; i++) ast_destroy_node(statements[i]); 
        KRT_FREE(statements); 
        return NULL; 
    }
    node->data.block.statements = statements;
    node->data.block.statement_count = count;
    return node;
}

ASTNode* parser_parse_statement(Parser* parser) {
    Token token = parser->current_token;

    switch (token.type) {
        case TOKEN_VAR:
        case TOKEN_LET:
            return parser_parse_variable_declaration(parser);

        case TOKEN_IF:
            return parser_parse_if_statement(parser);

        case TOKEN_WHILE:
            return parser_parse_while_statement(parser);

        case TOKEN_FOR:
            return parser_parse_for_statement(parser);

        case TOKEN_RETURN:
            return parser_parse_return_statement(parser);

        case TOKEN_PRINT:
            return parser_parse_print_statement(parser);

        case TOKEN_LEFT_BRACE:
            return parser_parse_block(parser);

        default: {
            ASTNode* expr = parser_parse_expression(parser);
            if (!expr) return NULL;
            return parser_parse_assignment_from_left(parser, expr);
        }
    }
}