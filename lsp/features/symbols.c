#include "symbols.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "../../../src/compiler/frontend/lexer/tokenizer.h"
#include "../../../src/compiler/frontend/parser/parser.h"
#include "../../../src/compiler/frontend/parser/ast.h"
#include "../../../src/compiler/frontend/semantic/symbol_table.h"

LspSymbolList* lsp_symbol_list_create(void) {
    LspSymbolList* list = (LspSymbolList*)calloc(1, sizeof(LspSymbolList));
    if (!list) return NULL;
    list->capacity = 64;
    list->items = (LspSymbolInfo*)calloc(list->capacity, sizeof(LspSymbolInfo));
    if (!list->items) {
        free(list);
        return NULL;
    }
    return list;
}

void lsp_symbol_list_destroy(LspSymbolList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        lsp_symbol_info_destroy(&list->items[i]);
    }
    free(list->items);
    free(list);
}

void lsp_symbol_list_add(LspSymbolList* list, const LspSymbolInfo* symbol) {
    if (!list || !symbol) return;
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        LspSymbolInfo* new_items = (LspSymbolInfo*)realloc(list->items, list->capacity * sizeof(LspSymbolInfo));
        if (!new_items) return;
        list->items = new_items;
    }
    
    list->items[list->count] = *symbol;
    list->count++;
}

void lsp_symbol_info_destroy(LspSymbolInfo* info) {
    if (!info) return;
    free(info->name);
    free(info->uri);
    free(info->detail);
    free(info->documentation);
}

static void extract_symbols_from_ast(ASTNode* node, const char* uri, LspSymbolList* list, int depth) {
    if (!node || depth > 100) return;
    
    switch (node->type) {
        case AST_FUNCTION_DECLARATION: {
            LspSymbolInfo sym = {0};
            sym.name = strdup(node->data.function_decl.name);
            sym.uri = strdup(uri);
            sym.line = 0;
            sym.character = 0;
            sym.kind = 12;
            sym.detail = strdup("Function");
            lsp_symbol_list_add(list, &sym);
            break;
        }
        case AST_VARIABLE_DECLARATION: {
            LspSymbolInfo sym = {0};
            sym.name = strdup(node->data.variable_decl.name);
            sym.uri = strdup(uri);
            sym.line = 0;
            sym.character = 0;
            sym.kind = 13;
            sym.detail = strdup("Variable");
            lsp_symbol_list_add(list, &sym);
            break;
        }
        case AST_CLASS_DECLARATION: {
            LspSymbolInfo sym = {0};
            sym.name = strdup(node->data.class_decl.name);
            sym.uri = strdup(uri);
            sym.line = 0;
            sym.character = 0;
            sym.kind = 5;
            sym.detail = strdup("Class");
            lsp_symbol_list_add(list, &sym);
            break;
        }
        case AST_NAMESPACE_DECLARATION: {
            LspSymbolInfo sym = {0};
            sym.name = strdup(node->data.namespace_decl.name);
            sym.uri = strdup(uri);
            sym.line = 0;
            sym.character = 0;
            sym.kind = 3;
            sym.detail = strdup("Namespace");
            lsp_symbol_list_add(list, &sym);
            break;
        }
        default:
            break;
    }
    
}

LspSymbolList* lsp_document_symbols(const char* content, const char* uri) {
    if (!content || !uri) return NULL;
    
    LspSymbolList* list = lsp_symbol_list_create();
    if (!list) return NULL;
    
    Lexer* lexer = lexer_create(content);
    if (!lexer) {
        lsp_symbol_list_destroy(list);
        return NULL;
    }
    
    Parser* parser = parser_create(lexer);
    if (!parser) {
        lexer_destroy(lexer);
        lsp_symbol_list_destroy(list);
        return NULL;
    }
    
    ASTNode* ast = parser_parse(parser);
    if (ast) {
        extract_symbols_from_ast(ast, uri, list, 0);
        ast_destroy_node(ast);
    }
    
    parser_destroy(parser);
    lexer_destroy(lexer);
    
    return list;
}

LspSymbolList* lsp_workspace_symbols(const char* query) {
    (void)query;
    return NULL;
}

LspSymbolInfo* lsp_find_symbol_at_position(const char* content, int line, int col) {
    if (!content) return NULL;
    
    const char** lines = NULL;
    int line_count = 0;
    
    const char* p = content;
    while (*p) {
        if (*p == '\n') line_count++;
        p++;
    }
    line_count++;
    
    lines = (const char**)malloc(line_count * sizeof(const char*));
    if (!lines) return NULL;
    
    p = content;
    int idx = 0;
    lines[idx++] = p;
    while (*p) {
        if (*p == '\n') {
            lines[idx++] = p + 1;
        }
        p++;
    }
    
    if (line >= line_count) {
        free(lines);
        return NULL;
    }
    
    const char* line_start = lines[line];
    const char* line_end = strchr(line_start, '\n');
    if (!line_end) line_end = line_start + strlen(line_start);
    
    size_t line_len = line_end - line_start;
    char* line_text = (char*)malloc(line_len + 1);
    if (!line_text) {
        free(lines);
        return NULL;
    }
    strncpy(line_text, line_start, line_len);
    line_text[line_len] = '\0';
    free(lines);
    
    int start = col;
    while (start > 0 && (isalnum((unsigned char)line_text[start - 1]) || line_text[start - 1] == '_')) {
        start--;
    }
    int end = col;
    while (end < (int)line_len && (isalnum((unsigned char)line_text[end]) || line_text[end] == '_')) {
        end++;
    }
    
    if (end <= start) {
        free(line_text);
        return NULL;
    }
    
    char* word = (char*)malloc(end - start + 1);
    if (!word) {
        free(line_text);
        return NULL;
    }
    strncpy(word, line_text + start, end - start);
    word[end - start] = '\0';
    free(line_text);
    
    LspSymbolInfo* info = (LspSymbolInfo*)calloc(1, sizeof(LspSymbolInfo));
    if (!info) {
        free(word);
        return NULL;
    }
    info->name = word;
    info->kind = 1;
    
    return info;
}

LspSymbolInfo* lsp_find_symbol_definition(const char* content, const char* symbol_name) {
    if (!content || !symbol_name) return NULL;
    
    Lexer* lexer = lexer_create(content);
    if (!lexer) return NULL;
    
    Parser* parser = parser_create(lexer);
    if (!parser) {
        lexer_destroy(lexer);
        return NULL;
    }
    
    ASTNode* ast = parser_parse(parser);
    if (!ast) {
        parser_destroy(parser);
        lexer_destroy(lexer);
        return NULL;
    }
    
    LspSymbolInfo* result = NULL;
    
    ast_destroy_node(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    
    return result;
}

char* lsp_symbol_kind_to_string(int kind) {
    switch (kind) {
        case 1: return "File";
        case 2: return "Module";
        case 3: return "Namespace";
        case 4: return "Package";
        case 5: return "Class";
        case 6: return "Method";
        case 7: return "Property";
        case 8: return "Field";
        case 9: return "Constructor";
        case 10: return "Enum";
        case 11: return "Interface";
        case 12: return "Function";
        case 13: return "Variable";
        case 14: return "Constant";
        case 15: return "String";
        case 16: return "Number";
        case 17: return "Boolean";
        case 18: return "Array";
        case 19: return "Object";
        case 20: return "Key";
        case 21: return "Null";
        case 22: return "EnumMember";
        case 23: return "Struct";
        case 24: return "Event";
        case 25: return "Operator";
        case 26: return "TypeParameter";
        default: return "Unknown";
    }
}

int lsp_symbol_type_to_kind(const char* type) {
    if (!type) return 1;
    if (strcmp(type, "function") == 0) return 12;
    if (strcmp(type, "variable") == 0) return 13;
    if (strcmp(type, "class") == 0) return 5;
    if (strcmp(type, "namespace") == 0) return 3;
    if (strcmp(type, "struct") == 0) return 23;
    if (strcmp(type, "enum") == 0) return 10;
    if (strcmp(type, "interface") == 0) return 11;
    return 1;
}