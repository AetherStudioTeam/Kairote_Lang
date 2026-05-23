#include "diagnostics.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "../../../src/compiler/frontend/lexer/tokenizer.h"
#include "../../../src/compiler/frontend/parser/parser.h"
#include "../../../src/compiler/frontend/parser/ast.h"
#include "../../../src/compiler/frontend/semantic/symbol_table.h"
#include "../lsp_log.h"

static const char* KrtKeywords[] = {
    "function", "var", "if", "else", "while", "for", "foreach", "in",
    "return", "print", "true", "false", "and", "or", "not", "new", "delete",
    "class", "struct", "interface", "enum", "namespace", "this", "base",
    "public", "private", "protected", "static", "virtual", "abstract", "override",
    "using", "import", "package", "console", "try", "catch", "finally", "throw",
    "template", "typename", "where", "switch", "case", "break", "continue", "default",
    "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
    "float32", "float64", "bool", "char", "void", "string",
    NULL
};

static int is_keyword(const char* word) {
    for (int i = 0; KrtKeywords[i]; i++) {
        if (strcmp(KrtKeywords[i], word) == 0) return 1;
    }
    return 0;
}

LspDiagnosticList* lsp_analyze_document(const char* uri, const char* content) {
    if (!content) return NULL;
    
    LspDiagnosticList* list = lsp_diagnostic_list_create();
    if (!list) return NULL;
    
    Lexer* lexer = lexer_create(content);
    if (!lexer) {
        lsp_diagnostic_list_destroy(list);
        return NULL;
    }
    
    Token prev_token;
    prev_token.type = TOKEN_EOF;
    prev_token.value = NULL;
    prev_token.line = 0;
    prev_token.column = 0;
    
    Token token;
    do {
        token = lexer_next_token(lexer);
        
        if (token.type == TOKEN_UNKNOWN) {
            LspDiagnostic diag = {0};
            diag.start_line = token.line - 1;
            diag.start_char = token.column - 1;
            diag.end_line = token.line - 1;
            diag.end_char = token.column;
            diag.severity = LSP_SEVERITY_ERROR;
            diag.source = strdup("Kairote Lang");
            diag.message = strdup("Unknown token");
            lsp_diagnostic_list_add(list, &diag);
        }
        
        if (prev_token.type == TOKEN_IDENTIFIER && token.type == TOKEN_IDENTIFIER) {
            LspDiagnostic diag = {0};
            diag.start_line = token.line - 1;
            diag.start_char = token.column - 1;
            diag.end_line = token.line - 1;
            diag.end_char = token.column + (int)strlen(token.value);
            diag.severity = LSP_SEVERITY_ERROR;
            diag.source = strdup("Kairote Lang");
            diag.message = strdup("Missing semicolon or operator between identifiers");
            lsp_diagnostic_list_add(list, &diag);
        }
        
        if (prev_token.value) {
            free(prev_token.value);
        }
        prev_token = token;
        prev_token.value = token.value ? strdup(token.value) : NULL;
        
        if (token.value && token.type == TOKEN_IDENTIFIER) {
            free(token.value);
        }
    } while (token.type != TOKEN_EOF);
    
    if (prev_token.value) {
        free(prev_token.value);
    }
    
    lexer_reset(lexer);
    Parser* parser = parser_create(lexer);
    if (parser) {
        ASTNode* ast = parser_parse(parser);
        if (!ast) {
            LspDiagnostic diag = {0};
            diag.start_line = 0;
            diag.start_char = 0;
            diag.end_line = 0;
            diag.end_char = 1;
            diag.severity = LSP_SEVERITY_ERROR;
            diag.source = strdup("Kairote Lang");
            diag.message = strdup("Parse error");
            lsp_diagnostic_list_add(list, &diag);
        } else {
            SymbolTable* sym_table = symbol_table_create();
            if (sym_table) {
                
                int errors = symbol_table_get_error_count(sym_table);
                if (errors > 0) {
                    LSP_LOG_DEBUG("Symbol table has %d errors", errors);
                }
                
                symbol_table_destroy(sym_table);
            }
            ast_destroy_node(ast);
        }
        parser_destroy(parser);
    }
    
    lexer_destroy(lexer);
    
    LSP_LOG_DEBUG("Analysis complete: %d diagnostics", list->count);
    
    return list;
}

LspCompletionList* lsp_get_completions(const char* content, int line, int col) {
    if (!content) return NULL;
    
    LspCompletionList* list = lsp_completion_list_create();
    if (!list) return NULL;
    
    for (int i = 0; KrtKeywords[i]; i++) {
        LspCompletionItem item = {0};
        item.label = strdup(KrtKeywords[i]);
        item.kind = LSP_COMPLETION_ITEM_KIND_KEYWORD;
        item.insert_text = strdup(KrtKeywords[i]);
        item.insert_text_format = LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT;
        item.detail = strdup("Kairote Lang keyword");
        lsp_completion_list_add(list, &item);
    }
    
    Lexer* lexer = lexer_create(content);
    if (lexer) {
        Token token;
        do {
            token = lexer_next_token(lexer);
            if (token.type == TOKEN_IDENTIFIER && token.value) {
                int exists = 0;
                for (int i = 0; i < list->count; i++) {
                    if (list->items[i].label && strcmp(list->items[i].label, token.value) == 0) {
                        exists = 1;
                        break;
                    }
                }
                if (!exists) {
                    LspCompletionItem item = {0};
                    item.label = strdup(token.value);
                    item.kind = LSP_COMPLETION_ITEM_KIND_VARIABLE;
                    item.insert_text = strdup(token.value);
                    item.insert_text_format = LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT;
                    item.detail = strdup("Symbol");
                    lsp_completion_list_add(list, &item);
                }
            }
            if (token.value) free(token.value);
        } while (token.type != TOKEN_EOF);
        lexer_destroy(lexer);
    }
    
    return list;
}

char* lsp_get_hover_info(const char* content, int line, int col) {
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
    
    char* result = NULL;
    if (is_keyword(word)) {
        result = (char*)malloc(strlen(word) + 64);
        if (result) {
            sprintf(result, "**%s**\n\nKairote Lang keyword", word);
        }
    } else {
        Lexer* lexer = lexer_create(content);
        if (lexer) {
            Parser* parser = parser_create(lexer);
            if (parser) {
                ASTNode* ast = parser_parse(parser);
                if (ast) {
                    
                    ast_destroy_node(ast);
                }
                parser_destroy(parser);
            }
            lexer_destroy(lexer);
        }
        
        result = (char*)malloc(strlen(word) + 64);
        if (result) {
            sprintf(result, "**%s**\n\nSymbol", word);
        }
    }
    
    free(word);
    return result;
}

LspLocation* lsp_get_definition(const char* content, int line, int col) {
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
    
    LspLocation* location = NULL;
    
    Lexer* lexer = lexer_create(content);
    if (lexer) {
        Parser* parser = parser_create(lexer);
        if (parser) {
            ASTNode* ast = parser_parse(parser);
            if (ast) {
                
                ast_destroy_node(ast);
            }
            parser_destroy(parser);
        }
        lexer_destroy(lexer);
    }
    
    free(word);
    return location;
}