#include "formatting.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int count_leading_spaces(const char* line) {
    int count = 0;
    while (*line && isspace((unsigned char)*line) && *line != '\n') {
        if (*line == ' ') count++;
        else if (*line == '\t') count += 4;
        line++;
    }
    return count;
}

static char* indent_line(const char* line, int indent_level, int use_spaces, int tab_size) {
    int content_len = 0;
    const char* p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    content_len = strlen(p);
    
    int indent_chars = use_spaces ? indent_level * tab_size : indent_level;
    char* result = (char*)malloc(indent_chars + content_len + 1);
    if (!result) return NULL;
    
    if (use_spaces) {
        memset(result, ' ', indent_level * tab_size);
        result[indent_level * tab_size] = '\0';
    } else {
        memset(result, '\t', indent_level);
        result[indent_level] = '\0';
    }
    
    strcat(result, p);
    return result;
}

char* lsp_format_document(const char* content, const LspFormatOptions* options) {
    if (!content || !options) return NULL;
    
    size_t content_len = strlen(content);
    char* result = (char*)malloc(content_len * 2 + 1);
    if (!result) return NULL;
    result[0] = '\0';
    
    int indent_level = 0;
    const char* p = content;
    
    while (*p) {
        const char* line_start = p;
        while (*p && *p != '\n') p++;
        size_t line_len = p - line_start;
        
        char* line = (char*)malloc(line_len + 1);
        if (!line) {
            free(result);
            return NULL;
        }
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';
        
        const char* trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        
        if (strstr(trimmed, "}") == trimmed) {
            indent_level--;
            if (indent_level < 0) indent_level = 0;
        }
        
        if (strlen(trimmed) > 0 || !options->trim_trailing_whitespace) {
            char* formatted = indent_line(trimmed, indent_level, options->use_spaces, options->tab_size);
            if (formatted) {
                strcat(result, formatted);
                free(formatted);
            }
        }
        strcat(result, "\n");
        
        if (strstr(trimmed, "{") == trimmed || 
            strstr(trimmed, "function") == trimmed ||
            strstr(trimmed, "if") == trimmed ||
            strstr(trimmed, "while") == trimmed ||
            strstr(trimmed, "for") == trimmed ||
            strstr(trimmed, "class") == trimmed ||
            strstr(trimmed, "namespace") == trimmed) {
            indent_level++;
        }
        
        free(line);
        
        if (*p == '\n') p++;
    }
    
    if (!options->insert_final_newline && strlen(result) > 0) {
        size_t len = strlen(result);
        if (result[len - 1] == '\n') {
            result[len - 1] = '\0';
        }
    }
    
    return result;
}

char* lsp_format_range(const char* content, int start_line, int start_col, int end_line, int end_col, const LspFormatOptions* options) {
    (void)start_col;
    (void)end_col;
    if (!content || !options) return NULL;
    
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
    
    if (start_line < 0) start_line = 0;
    if (end_line >= line_count) end_line = line_count - 1;
    
    size_t result_size = strlen(content) + 1;
    char* result = (char*)malloc(result_size);
    if (!result) {
        free(lines);
        return NULL;
    }
    result[0] = '\0';
    
    for (int i = 0; i < start_line; i++) {
        if (i < line_count - 1) {
            size_t len = lines[i + 1] - lines[i] - 1;
            strncat(result, lines[i], len);
            strcat(result, "\n");
        } else {
            strcat(result, lines[i]);
        }
    }
    
    int indent_level = 0;
    for (int i = start_line; i <= end_line && i < line_count; i++) {
        const char* line = lines[i];
        size_t line_len;
        if (i < line_count - 1) {
            line_len = lines[i + 1] - line - 1;
        } else {
            line_len = strlen(line);
        }
        
        char* line_copy = (char*)malloc(line_len + 1);
        if (!line_copy) continue;
        strncpy(line_copy, line, line_len);
        line_copy[line_len] = '\0';
        
        const char* trimmed = line_copy;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        
        if (strstr(trimmed, "}") == trimmed) {
            indent_level--;
            if (indent_level < 0) indent_level = 0;
        }
        
        if (strlen(trimmed) > 0 || !options->trim_trailing_whitespace) {
            char* formatted = indent_line(trimmed, indent_level, options->use_spaces, options->tab_size);
            if (formatted) {
                strcat(result, formatted);
                free(formatted);
            }
        }
        strcat(result, "\n");
        
        if (strstr(trimmed, "{") == trimmed || 
            strstr(trimmed, "function") == trimmed ||
            strstr(trimmed, "if") == trimmed ||
            strstr(trimmed, "while") == trimmed ||
            strstr(trimmed, "for") == trimmed ||
            strstr(trimmed, "class") == trimmed ||
            strstr(trimmed, "namespace") == trimmed) {
            indent_level++;
        }
        
        free(line_copy);
    }
    
    for (int i = end_line + 1; i < line_count; i++) {
        if (i < line_count - 1) {
            size_t len = lines[i + 1] - lines[i] - 1;
            strncat(result, lines[i], len);
            strcat(result, "\n");
        } else {
            strcat(result, lines[i]);
        }
    }
    
    free(lines);
    return result;
}

char* lsp_format_on_type(const char* content, int line, int col, char ch, const LspFormatOptions* options) {
    (void)col;
    if (!content || !options) return NULL;
    
    if (ch == ';' || ch == '}') {
        return lsp_format_range(content, line, 0, line, 0, options);
    }
    
    return strdup(content);
}