#include "lsp_messages.h"
#include <stdio.h>
#include <string.h>

char* lsp_position_to_json(const LspPosition* pos) {
    if (!pos) return strdup("null");
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\"line\":%d,\"character\":%d}", pos->line, pos->character);
    return strdup(buffer);
}

char* lsp_range_to_json(const LspRange* range) {
    if (!range) return strdup("null");
    char* start = lsp_position_to_json(&range->start);
    char* end = lsp_position_to_json(&range->end);
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "{\"start\":%s,\"end\":%s}", start, end);
    free(start);
    free(end);
    return strdup(buffer);
}

char* lsp_location_to_json(const LspLocation* loc) {
    if (!loc) return strdup("null");
    char* range = lsp_range_to_json(&loc->range);
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "{\"uri\":\"%s\",\"range\":%s}", loc->uri ? loc->uri : "", range);
    free(range);
    return strdup(buffer);
}

char* lsp_diagnostic_to_json(const LspDiagnostic* diag) {
    if (!diag) return strdup("null");
    
    char* escaped_message = lsp_json_escape(diag->message ? diag->message : "");
    char buffer[4096];
    
    snprintf(buffer, sizeof(buffer),
        "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},"
        "\"severity\":%d,"
        "\"message\":\"%s\"",
        diag->start_line, diag->start_char,
        diag->end_line, diag->end_char,
        diag->severity,
        escaped_message);
    
    if (diag->code) {
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"code\":\"%s\"", diag->code);
    }
    
    if (diag->source) {
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"source\":\"%s\"", diag->source);
    }
    
    strcat(buffer, "}");
    free(escaped_message);
    return strdup(buffer);
}

char* lsp_diagnostic_list_to_json(const LspDiagnosticList* list) {
    if (!list || list->count == 0) return strdup("[]");
    
    size_t buffer_size = 4096;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return strdup("[]");
    
    strcpy(buffer, "[");
    
    for (int i = 0; i < list->count; i++) {
        char* diag_json = lsp_diagnostic_to_json(&list->items[i]);
        size_t needed = strlen(buffer) + strlen(diag_json) + 10;
        if (needed > buffer_size) {
            buffer_size = needed * 2;
            buffer = (char*)realloc(buffer, buffer_size);
            if (!buffer) return strdup("[]");
        }
        if (i > 0) strcat(buffer, ",");
        strcat(buffer, diag_json);
        free(diag_json);
    }
    
    strcat(buffer, "]");
    return buffer;
}

char* lsp_completion_item_to_json(const LspCompletionItem* item) {
    if (!item) return strdup("null");
    
    char* escaped_label = lsp_json_escape(item->label ? item->label : "");
    char buffer[4096];
    
    snprintf(buffer, sizeof(buffer),
        "{\"label\":\"%s\"",
        escaped_label);
    
    if (item->detail) {
        char* escaped = lsp_json_escape(item->detail);
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"detail\":\"%s\"", escaped);
        free(escaped);
    }
    
    if (item->documentation) {
        char* escaped = lsp_json_escape(item->documentation);
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"documentation\":\"%s\"", escaped);
        free(escaped);
    }
    
    if (item->insert_text) {
        char* escaped = lsp_json_escape(item->insert_text);
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"insertText\":\"%s\"", escaped);
        free(escaped);
    }
    
    if (item->insert_text_format > 0) {
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"insertTextFormat\":%d", item->insert_text_format);
    }
    
    if (item->sort_text) {
        char* escaped = lsp_json_escape(item->sort_text);
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"sortText\":\"%s\"", escaped);
        free(escaped);
    }
    
    if (item->filter_text) {
        char* escaped = lsp_json_escape(item->filter_text);
        size_t len = strlen(buffer);
        snprintf(buffer + len, sizeof(buffer) - len, ",\"filterText\":\"%s\"", escaped);
        free(escaped);
    }
    
    strcat(buffer, "}");
    free(escaped_label);
    return strdup(buffer);
}

char* lsp_completion_list_to_json(const LspCompletionList* list) {
    if (!list) return strdup("{\"isIncomplete\":false,\"items\":[]}");
    
    size_t buffer_size = 8192;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return strdup("{\"isIncomplete\":false,\"items\":[]}");
    
    snprintf(buffer, buffer_size, "{\"isIncomplete\":%s,\"items\":[",
             list->is_incomplete ? "true" : "false");
    
    for (int i = 0; i < list->count; i++) {
        char* item_json = lsp_completion_item_to_json(&list->items[i]);
        size_t needed = strlen(buffer) + strlen(item_json) + 10;
        if (needed > buffer_size) {
            buffer_size = needed * 2;
            buffer = (char*)realloc(buffer, buffer_size);
            if (!buffer) return strdup("{\"isIncomplete\":false,\"items\":[]}");
        }
        if (i > 0) strcat(buffer, ",");
        strcat(buffer, item_json);
        free(item_json);
    }
    
    strcat(buffer, "]}");
    return buffer;
}

LspPosition* lsp_position_from_json(const char* json) {
    if (!json) return NULL;
    LspPosition* pos = (LspPosition*)malloc(sizeof(LspPosition));
    if (!pos) return NULL;
    
    pos->line = 0;
    pos->character = 0;
    
    const char* line_str = strstr(json, "\"line\":");
    if (line_str) {
        pos->line = atoi(line_str + 7);
    }
    
    const char* char_str = strstr(json, "\"character\":");
    if (char_str) {
        pos->character = atoi(char_str + 12);
    }
    
    return pos;
}

LspRange* lsp_range_from_json(const char* json) {
    if (!json) return NULL;
    LspRange* range = (LspRange*)malloc(sizeof(LspRange));
    if (!range) return NULL;
    
    const char* start_str = strstr(json, "\"start\":");
    if (start_str) {
        range->start.line = 0;
        range->start.character = 0;
        const char* line_str = strstr(start_str, "\"line\":");
        if (line_str) range->start.line = atoi(line_str + 7);
        const char* char_str = strstr(start_str, "\"character\":");
        if (char_str) range->start.character = atoi(char_str + 12);
    }
    
    const char* end_str = strstr(json, "\"end\":");
    if (end_str) {
        range->end.line = 0;
        range->end.character = 0;
        const char* line_str = strstr(end_str, "\"line\":");
        if (line_str) range->end.line = atoi(line_str + 7);
        const char* char_str = strstr(end_str, "\"character\":");
        if (char_str) range->end.character = atoi(char_str + 12);
    }
    
    return range;
}

LspLocation* lsp_location_from_json(const char* json) {
    if (!json) return NULL;
    LspLocation* loc = (LspLocation*)malloc(sizeof(LspLocation));
    if (!loc) return NULL;
    
    loc->uri = NULL;
    
    const char* uri_str = strstr(json, "\"uri\":\"");
    if (uri_str) {
        uri_str += 7;
        const char* end = strchr(uri_str, '"');
        if (end) {
            size_t len = end - uri_str;
            loc->uri = (char*)malloc(len + 1);
            if (loc->uri) {
                strncpy(loc->uri, uri_str, len);
                loc->uri[len] = '\0';
            }
        }
    }
    
    const char* range_str = strstr(json, "\"range\":");
    if (range_str) {
        LspRange* range = lsp_range_from_json(range_str + 8);
        if (range) {
            loc->range = *range;
            free(range);
        }
    }
    
    return loc;
}

LspDiagnostic* lsp_diagnostic_from_json(const char* json) {
    if (!json) return NULL;
    LspDiagnostic* diag = (LspDiagnostic*)calloc(1, sizeof(LspDiagnostic));
    if (!diag) return NULL;
    
    const char* range_str = strstr(json, "\"range\":");
    if (range_str) {
        LspRange* range = lsp_range_from_json(range_str + 8);
        if (range) {
            diag->start_line = range->start.line;
            diag->start_char = range->start.character;
            diag->end_line = range->end.line;
            diag->end_char = range->end.character;
            free(range);
        }
    }
    
    const char* severity_str = strstr(json, "\"severity\":");
    if (severity_str) {
        diag->severity = atoi(severity_str + 11);
    }
    
    return diag;
}

void lsp_position_destroy(LspPosition* pos) {
    free(pos);
}

void lsp_range_destroy(LspRange* range) {
    free(range);
}

void lsp_location_destroy(LspLocation* loc) {
    if (!loc) return;
    free(loc->uri);
    free(loc);
}

void lsp_diagnostic_destroy(LspDiagnostic* diag) {
    if (!diag) return;
    free(diag->code);
    free(diag->source);
    free(diag->message);
    free(diag);
}

void lsp_diagnostic_list_destroy(LspDiagnosticList* list) {
    if (!list) return;
    free(list->items);
    free(list);
}

void lsp_completion_item_destroy(LspCompletionItem* item) {
    if (!item) return;
    free(item->label);
    free(item->documentation);
    free(item->detail);
    free(item->sort_text);
    free(item->filter_text);
    free(item->insert_text);
}

void lsp_completion_list_destroy(LspCompletionList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        lsp_completion_item_destroy(&list->items[i]);
    }
    free(list->items);
    free(list);
}

LspDiagnosticList* lsp_diagnostic_list_create(void) {
    LspDiagnosticList* list = (LspDiagnosticList*)calloc(1, sizeof(LspDiagnosticList));
    if (!list) return NULL;
    list->capacity = 16;
    list->items = (LspDiagnostic*)calloc(list->capacity, sizeof(LspDiagnostic));
    if (!list->items) {
        free(list);
        return NULL;
    }
    return list;
}

void lsp_diagnostic_list_add(LspDiagnosticList* list, const LspDiagnostic* diag) {
    if (!list || !diag) return;
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = (LspDiagnostic*)realloc(list->items, list->capacity * sizeof(LspDiagnostic));
        if (!list->items) return;
    }
    
    list->items[list->count] = *diag;
    list->count++;
}

LspCompletionList* lsp_completion_list_create(void) {
    LspCompletionList* list = (LspCompletionList*)calloc(1, sizeof(LspCompletionList));
    if (!list) return NULL;
    list->capacity = 64;
    list->items = (LspCompletionItem*)calloc(list->capacity, sizeof(LspCompletionItem));
    if (!list->items) {
        free(list);
        return NULL;
    }
    return list;
}

void lsp_completion_list_add(LspCompletionList* list, const LspCompletionItem* item) {
    if (!list || !item) return;
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = (LspCompletionItem*)realloc(list->items, list->capacity * sizeof(LspCompletionItem));
        if (!list->items) return;
    }
    
    list->items[list->count] = *item;
    list->count++;
}