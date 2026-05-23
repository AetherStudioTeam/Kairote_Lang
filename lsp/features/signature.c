#include "signature.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "../../../src/compiler/frontend/lexer/tokenizer.h"
#include "../../../src/compiler/frontend/parser/parser.h"
#include "../../../src/compiler/frontend/parser/ast.h"

static char* extract_function_name_at_position(const char* content, int line, int col) {
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
    int current_col = 0;
    
    while (*line_start && current_col < col) {
        if (*line_start == '(') {
            
            const char* name_end = line_start;
            while (name_end > lines[line] && isspace((unsigned char)*(name_end - 1))) name_end--;
            
            const char* name_start = name_end;
            while (name_start > lines[line] && (isalnum((unsigned char)*(name_start - 1)) || *(name_start - 1) == '_')) {
                name_start--;
            }
            
            size_t len = name_end - name_start;
            if (len > 0) {
                char* name = (char*)malloc(len + 1);
                if (name) {
                    strncpy(name, name_start, len);
                    name[len] = '\0';
                    free(lines);
                    return name;
                }
            }
        }
        line_start++;
        current_col++;
    }
    
    free(lines);
    return NULL;
}

LspSignatureHelp* lsp_get_signature_help(const char* content, int line, int col) {
    if (!content) return NULL;
    
    char* func_name = extract_function_name_at_position(content, line, col);
    if (!func_name) return NULL;
    
    LspSignatureHelp* help = (LspSignatureHelp*)calloc(1, sizeof(LspSignatureHelp));
    if (!help) {
        free(func_name);
        return NULL;
    }
    
    help->signatures = (LspSignatureInfo*)calloc(1, sizeof(LspSignatureInfo));
    if (!help->signatures) {
        free(help);
        free(func_name);
        return NULL;
    }
    help->signature_count = 1;
    help->active_signature = 0;
    help->active_parameter = 0;
    
    Lexer* lexer = lexer_create(content);
    if (lexer) {
        Parser* parser = parser_create(lexer);
        if (parser) {
            ASTNode* ast = parser_parse(parser);
            if (ast) {
                
                help->signatures[0].label = (char*)malloc(strlen(func_name) + 32);
                if (help->signatures[0].label) {
                    sprintf(help->signatures[0].label, "%s(...)", func_name);
                }
                help->signatures[0].documentation = strdup("Function");
                
                ast_destroy_node(ast);
            }
            parser_destroy(parser);
        }
        lexer_destroy(lexer);
    }
    
    if (!help->signatures[0].label) {
        help->signatures[0].label = strdup(func_name);
        help->signatures[0].documentation = strdup("Function");
    }
    
    free(func_name);
    return help;
}

void lsp_signature_help_destroy(LspSignatureHelp* help) {
    if (!help) return;
    for (int i = 0; i < help->signature_count; i++) {
        lsp_signature_info_destroy(&help->signatures[i]);
    }
    free(help->signatures);
    free(help);
}

void lsp_signature_info_destroy(LspSignatureInfo* sig) {
    if (!sig) return;
    free(sig->label);
    free(sig->documentation);
    for (int i = 0; i < sig->parameter_count; i++) {
        free(sig->parameters[i].label);
        free(sig->parameters[i].documentation);
    }
    free(sig->parameters);
}

char* lsp_signature_help_to_json(const LspSignatureHelp* help) {
    if (!help || help->signature_count == 0) return strdup("null");
    
    size_t buffer_size = 4096;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return strdup("null");
    
    int offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, 
        "{\"signatures\":[", help->active_signature, help->active_parameter);
    
    for (int i = 0; i < help->signature_count; i++) {
        const LspSignatureInfo* sig = &help->signatures[i];
        
        char* escaped_label = lsp_json_escape(sig->label ? sig->label : "");
        char* escaped_doc = lsp_json_escape(sig->documentation ? sig->documentation : "");
        
        if (i > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",");
        }
        
        offset += snprintf(buffer + offset, buffer_size - offset,
            "{\"label\":\"%s\",\"documentation\":\"%s\",\"parameters\":[",
            escaped_label, escaped_doc);
        
        free(escaped_label);
        free(escaped_doc);
        
        for (int j = 0; j < sig->parameter_count; j++) {
            char* escaped_p_label = lsp_json_escape(sig->parameters[j].label ? sig->parameters[j].label : "");
            char* escaped_p_doc = lsp_json_escape(sig->parameters[j].documentation ? sig->parameters[j].documentation : "");
            
            if (j > 0) {
                offset += snprintf(buffer + offset, buffer_size - offset, ",");
            }
            
            offset += snprintf(buffer + offset, buffer_size - offset,
                "{\"label\":\"%s\",\"documentation\":\"%s\"}",
                escaped_p_label, escaped_p_doc);
            
            free(escaped_p_label);
            free(escaped_p_doc);
        }
        
        offset += snprintf(buffer + offset, buffer_size - offset, "]}");
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\"activeSignature\":%d,\"activeParameter\":%d}",
        help->active_signature, help->active_parameter);
    
    return buffer;
}