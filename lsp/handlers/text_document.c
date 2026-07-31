#include "text_document.h"
#include <stdio.h>
#include <string.h>
#include "../features/diagnostics.h"
#include "../features/symbols.h"
#include "../features/signature.h"
#include "../features/formatting.h"
#include "../protocol/json_rpc.h"
#include "../lsp_log.h"

static char* extract_string_value(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* pos = strstr(json, pattern);
    if (!pos) return NULL;
    
    pos += strlen(pattern);
    const char* end = pos;
    while (*end && *end != '"') end++;
    
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, pos, len);
    result[len] = '\0';
    return result;
}

static int extract_int_value(const char* json, const char* key, int default_val) {
    if (!json || !key) return default_val;
    
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* pos = strstr(json, pattern);
    if (!pos) return default_val;
    
    return atoi(pos + strlen(pattern));
}

static int has_range_in_change(const char* change) {
    return strstr(change, "\"range\"") != NULL;
}

static int parse_range(const char* json, int* start_line, int* start_char, int* end_line, int* end_char) {
    if (!json) return 0;
    
    const char* range = strstr(json, "\"range\":");
    if (!range) return 0;
    range += 8;
    
    const char* start = strstr(range, "\"start\":");
    if (start) {
        *start_line = extract_int_value(start, "line", 0);
        *start_char = extract_int_value(start, "character", 0);
    }
    
    const char* end = strstr(range, "\"end\":");
    if (end) {
        *end_line = extract_int_value(end, "line", 0);
        *end_char = extract_int_value(end, "character", 0);
    }
    
    return 1;
}

LspMessage* lsp_handle_text_document_did_open(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    char* language_id = extract_string_value(params, "languageId");
    const char* text_doc = extract_string_value(params, "textDocument");

    int version = extract_int_value(text_doc, "version", 0);
    char* text = extract_string_value(text_doc, "text");

    if (uri && text) {
        LSP_LOG_INFO("Opening document: %s (version %d)", uri, version);
        lsp_document_store_open(server->documents, uri, language_id, version, text);
        
        LspDiagnosticList* diagnostics = lsp_analyze_document(uri, text);
        if (diagnostics) {
            lsp_publish_diagnostics(server, uri, diagnostics);
            lsp_diagnostic_list_destroy(diagnostics);
        }
    }
    
    free(uri);
    free(language_id);
    free(text);
    
    return NULL;
}

LspMessage* lsp_handle_text_document_did_change(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");

    const char* changes_start = strstr(params, "\"contentChanges\":");
    if (!changes_start || !uri) {
        free(uri);
        return NULL;
    }
    changes_start += 17;
    
    while (*changes_start && isspace((unsigned char)*changes_start)) changes_start++;
    if (*changes_start != '[') {
        free(uri);
        return NULL;
    }
    changes_start++;
    
    LspTextChange changes[16];
    int change_count = 0;
    const char* ptr = changes_start;
    
    while (*ptr && *ptr != ']' && change_count < 16) {
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (*ptr != '{') break;
        
        int start_line = 0, start_char = 0, end_line = 0, end_char = 0;
        int has_range = 0;
        
        const char* range_pos = strstr(ptr, "\"range\":");
        if (range_pos) {
            has_range = parse_range(ptr, &start_line, &start_char, &end_line, &end_char);
        }
        
        char* text = extract_string_value(ptr, "text");
        
        changes[change_count].start_line = start_line;
        changes[change_count].start_char = start_char;
        changes[change_count].end_line = end_line;
        changes[change_count].end_char = end_char;
        changes[change_count].text = text ? text : strdup("");
        change_count++;
        
        const char* next = strchr(ptr, '}');
        if (!next) break;
        ptr = next + 1;
        
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == ',') ptr++;
    }
    
    if (change_count > 0) {
        if (changes[0].start_line == 0 && changes[0].start_char == 0 && 
            changes[0].end_line == 0 && changes[0].end_char == 0 &&

        }
        
        char* new_content = lsp_document_store_get_content(server->documents, uri);
        if (new_content) {
            LspDiagnosticList* diagnostics = lsp_analyze_document(uri, new_content);
            if (diagnostics) {
                lsp_publish_diagnostics(server, uri, diagnostics);
                lsp_diagnostic_list_destroy(diagnostics);
            }
        }
    }
    
    for (int i = 0; i < change_count; i++) {
        free(changes[i].text);
    }
    free(uri);
    
    return NULL;
}

LspMessage* lsp_handle_text_document_did_close(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");

    if (uri) {
        LSP_LOG_INFO("Closing document: %s", uri);
        lsp_document_store_close(server->documents, uri);

        char params_json[1024];
        snprintf(params_json, sizeof(params_json), "{\"uri\":\"%s\",\"diagnostics\":[]}", uri);
        lsp_send_notification(server, "textDocument/publishDiagnostics", params_json);
    }

    free(uri);
    return NULL;
}

LspMessage* lsp_handle_text_document_completion(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    if (!uri) {
        return lsp_message_create_response(id, "{\"items\":[]}");
    }

    LspDocument* doc = lsp_document_store_get(server->documents, uri);
    if (!doc || !doc->content) {
        free(uri);
        return lsp_message_create_response(id, "{\"items\":[]}");
    }

    int line = 0, character = 0;
    const char* pos = strstr(params, "\"position\":");
    if (pos) {
        line = extract_int_value(pos, "line", 0);
        character = extract_int_value(pos, "character", 0);
    }

    LspCompletionList* completions = lsp_get_completions(doc, line, character);
    if (!completions) {
        free(uri);
        return lsp_message_create_response(id, "{\"items\":[]}");
    }
    
    char* result = lsp_completion_list_to_json(completions);
    lsp_completion_list_destroy(completions);
    free(uri);
    
    char full_result[16384];
    snprintf(full_result, sizeof(full_result), "{\"isIncomplete\":false,\"items\":%s}", result);
    free(result);
    
    return lsp_message_create_response(id, full_result);
}

LspMessage* lsp_handle_text_document_hover(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    char* hover_content = lsp_get_hover_content(server, uri);

    free(uri);

    if (!hover_content) {
        return lsp_message_create_response(id, "null");
    }
    
    char* escaped = lsp_json_escape(hover_content);
    free(hover_content);
    
    char result[4096];
    snprintf(result, sizeof(result),
        "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
        escaped);
    free(escaped);
    
    return lsp_message_create_response(id, result);
}

LspMessage* lsp_handle_text_document_definition(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    LspLocation* location = lsp_get_definition(server, uri);

    free(uri);

    if (!location) {
        return lsp_message_create_response(id, "null");
    }
    
    char* result = lsp_location_to_json(location);
    lsp_location_destroy(location);
    
    LspMessage* response = lsp_message_create_response(id, result);
    free(result);
    return response;
}

LspMessage* lsp_handle_text_document_document_symbol(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    LspSymbolInfoList* symbols = lsp_get_document_symbols(uri);

    free(uri);

    if (!symbols || symbols->count == 0) {
        if (symbols) lsp_symbol_list_destroy(symbols);
        return lsp_message_create_response(id, "[]");
    }
    
    size_t buffer_size = 8192;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        lsp_symbol_list_destroy(symbols);
        return lsp_message_create_response(id, "[]");
    }
    
    strcpy(buffer, "[");
    
    for (int i = 0; i < symbols->count; i++) {
        LspSymbolInfo* sym = &symbols->items[i];
        char sym_json[1024];
        
        char* escaped_name = lsp_json_escape(sym->name ? sym->name : "");
        char* escaped_detail = lsp_json_escape(sym->detail ? sym->detail : "");
        
        snprintf(sym_json, sizeof(sym_json),
            "{\"name\":\"%s\",\"kind\":%d,\"location\":{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}},\"detail\":\"%s\"}",
            escaped_name, sym->kind, sym->uri ? sym->uri : "", 
            sym->line, sym->character, sym->line, sym->character + (int)strlen(sym->name ? sym->name : ""),
            escaped_detail);
        
        free(escaped_name);
        free(escaped_detail);
        
        size_t needed = strlen(buffer) + strlen(sym_json) + 10;
        if (needed > buffer_size) {
            buffer_size = needed * 2;
            buffer = (char*)realloc(buffer, buffer_size);
            if (!buffer) {
                lsp_symbol_list_destroy(symbols);
                return lsp_message_create_response(id, "[]");
            }
        }
        
        if (i > 0) strcat(buffer, ",");
        strcat(buffer, sym_json);
    }
    
    strcat(buffer, "]");
    
    LspMessage* response = lsp_message_create_response(id, buffer);
    
    free(buffer);
    lsp_symbol_list_destroy(symbols);
    
    return response;
}

LspMessage* lsp_handle_text_document_signature_help(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    LspSignatureHelp* help = lsp_get_signature_help(server, uri);

    free(uri);

    if (!help) {
        return lsp_message_create_response(id, "null");
    }
    
    char* result = lsp_signature_help_to_json(help);
    lsp_signature_help_destroy(help);
    
    LspMessage* response = lsp_message_create_response(id, result);
    free(result);
    return response;
}

static void parse_format_options(const char* params, LspFormatOptions* options) {
    options->tab_size = extract_int_value(params, "tabSize", 4);
    options->use_spaces = 1;
    const char* insert_spaces = strstr(params, "\"insertSpaces\":");
    if (insert_spaces) {
        if (strstr(insert_spaces, "false")) {
            options->use_spaces = 0;
        }
    }
    options->trim_trailing_whitespace = 1;
    options->insert_final_newline = 1;
}

LspMessage* lsp_handle_text_document_formatting(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    if (!uri) {
        return lsp_message_create_response(id, "null");
    }

    LspDocument* doc = lsp_server_get_document(server, uri);
    if (!doc) {
        free(uri);
        return lsp_message_create_response(id, "null");
    }

    LspFormatOptions options;
    parse_format_options(params, &options);

    char* formatted = lsp_format_document(doc->content, &options);
    free(uri);
    
    if (!formatted) {
        return lsp_message_create_response(id, "null");
    }
    
    char* escaped = lsp_json_escape(formatted);
    free(formatted);
    
    char result[16384];
    snprintf(result, sizeof(result),
        "[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":999999,\"character\":999999}},\"newText\":\"%s\"}]",
        escaped);
    free(escaped);
    
    return lsp_message_create_response(id, result);
}

LspMessage* lsp_handle_text_document_range_formatting(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    if (!uri) {
        return lsp_message_create_response(id, "null");
    }

    LspDocument* doc = lsp_server_get_document(server, uri);
    if (!doc) {
        free(uri);
        return lsp_message_create_response(id, "null");
    }

    int start_line = 0, start_char = 0, end_line = 0, end_char = 0;
    const char* range = strstr(params, "\"range\":");
    if (range) {
        start_line = extract_int_value(range, "line", 0);
        start_char = extract_int_value(range, "character", 0);
        const char* end = strstr(range, "\"end\":");
        if (end) {
            end_line = extract_int_value(end, "line", start_line);
            end_char = extract_int_value(end, "character", start_char);
        }
    }
    
    LspFormatOptions options;
    parse_format_options(params, &options);
    
    char* formatted = lsp_format_range(doc->content, start_line, start_char, end_line, end_char, &options);
    free(uri);
    
    if (!formatted) {
        return lsp_message_create_response(id, "null");
    }
    
    char* escaped = lsp_json_escape(formatted);
    free(formatted);
    
    char result[16384];
    snprintf(result, sizeof(result),
        "[{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},\"newText\":\"%s\"}]",
        start_line, start_char, end_line, end_char, escaped);
    free(escaped);
    
    return lsp_message_create_response(id, result);
}

LspMessage* lsp_handle_text_document_on_type_formatting(LspServer* server, const char* id, const char* params) {
    char* uri = extract_string_value(params, "uri");
    if (!uri) {
        return lsp_message_create_response(id, "null");
    }

    LspDocument* doc = lsp_server_get_document(server, uri);
    if (!doc) {
        free(uri);
        return lsp_message_create_response(id, "null");
    }

    int line = 0, character = 0;
    const char* pos = strstr(params, "\"position\":");
    if (pos) {
        line = extract_int_value(pos, "line", 0);
        character = extract_int_value(pos, "character", 0);
    }
    
    char ch = 0;
    const char* ch_pos = strstr(params, "\"ch\":");
    if (ch_pos) {
        ch_pos += 5;
        while (*ch_pos && isspace((unsigned char)*ch_pos)) ch_pos++;
        if (*ch_pos == '"') {
            ch = ch_pos[1];
        }
    }
    
    LspFormatOptions options;
    parse_format_options(params, &options);
    
    char* formatted = lsp_format_on_type(doc->content, line, character, ch, &options);
    free(uri);
    
    if (!formatted) {
        return lsp_message_create_response(id, "null");
    }
    
    char* escaped = lsp_json_escape(formatted);
    free(formatted);
    
    char result[16384];
    snprintf(result, sizeof(result),
        "[{\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":999999}},\"newText\":\"%s\"}]",
        line, line, escaped);
    free(escaped);
    
    return lsp_message_create_response(id, result);
}