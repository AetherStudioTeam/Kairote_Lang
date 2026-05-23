#include "json_rpc.h"
#include <ctype.h>

LspMessage* lsp_message_create(void) {
    LspMessage* msg = (LspMessage*)malloc(sizeof(LspMessage));
    if (!msg) return NULL;
    msg->jsonrpc = NULL;
    msg->method = NULL;
    msg->id = NULL;
    msg->id_is_number = 0;
    msg->id_number = 0;
    msg->params = NULL;
    msg->result = NULL;
    msg->error = NULL;
    msg->error_code = 0;
    return msg;
}

void lsp_message_destroy(LspMessage* msg) {
    if (!msg) return;
    free(msg->jsonrpc);
    free(msg->method);
    free(msg->id);
    free(msg->params);
    free(msg->result);
    free(msg->error);
    free(msg);
}

LspMessage* lsp_message_create_request(const char* id, const char* method, const char* params) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id = id ? strdup(id) : NULL;
    msg->method = method ? strdup(method) : NULL;
    msg->params = params ? strdup(params) : NULL;
    return msg;
}

LspMessage* lsp_message_create_request_num(int id, const char* method, const char* params) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id = NULL;
    msg->id_is_number = 1;
    msg->id_number = id;
    msg->method = method ? strdup(method) : NULL;
    msg->params = params ? strdup(params) : NULL;
    return msg;
}

LspMessage* lsp_message_create_response(const char* id, const char* result) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id = id ? strdup(id) : NULL;
    msg->result = result ? strdup(result) : NULL;
    return msg;
}

LspMessage* lsp_message_create_response_num(int id, const char* result) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id = NULL;
    msg->id_is_number = 1;
    msg->id_number = id;
    msg->result = result ? strdup(result) : NULL;
    return msg;
}

LspMessage* lsp_message_create_error(const char* id, int code, const char* message) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id = id ? strdup(id) : NULL;
    msg->error_code = code;
    if (message) {
        char* escaped = lsp_json_escape(message);
        size_t len = strlen(escaped) + 100;
        msg->error = (char*)malloc(len);
        if (msg->error) {
            snprintf(msg->error, len, "{\"code\":%d,\"message\":\"%s\"}", code, escaped);
        }
        free(escaped);
    }
    return msg;
}

LspMessage* lsp_message_create_error_num(int id, int code, const char* message) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->id_is_number = 1;
    msg->id_number = id;
    msg->error_code = code;
    if (message) {
        char* escaped = lsp_json_escape(message);
        size_t len = strlen(escaped) + 100;
        msg->error = (char*)malloc(len);
        if (msg->error) {
            snprintf(msg->error, len, "{\"code\":%d,\"message\":\"%s\"}", code, escaped);
        }
        free(escaped);
    }
    return msg;
}

LspMessage* lsp_message_create_notification(const char* method, const char* params) {
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    msg->jsonrpc = strdup(JSON_RPC_VERSION);
    msg->method = method ? strdup(method) : NULL;
    msg->params = params ? strdup(params) : NULL;
    return msg;
}

char* lsp_message_serialize(const LspMessage* msg) {
    if (!msg) return NULL;
    
    size_t buffer_size = 8192;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return NULL;
    
    int offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "{\"jsonrpc\":\"%s\"", msg->jsonrpc ? msg->jsonrpc : JSON_RPC_VERSION);
    
    if (msg->id || msg->id_is_number) {
        if (msg->id_is_number) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",\"id\":%d", msg->id_number);
        } else {
            offset += snprintf(buffer + offset, buffer_size - offset, ",\"id\":\"%s\"", msg->id);
        }
    }
    
    if (msg->method) {
        offset += snprintf(buffer + offset, buffer_size - offset, ",\"method\":\"%s\"", msg->method);
    }
    
    if (msg->params) {
        offset += snprintf(buffer + offset, buffer_size - offset, ",\"params\":%s", msg->params);
    }
    
    if (msg->result) {
        offset += snprintf(buffer + offset, buffer_size - offset, ",\"result\":%s", msg->result);
    }
    
    if (msg->error) {
        offset += snprintf(buffer + offset, buffer_size - offset, ",\"error\":%s", msg->error);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "}");
    
    return buffer;
}

static char* extract_json_string(const char* json, const char* key, int* end_pos);

static int extract_id(const char* json, LspMessage* msg) {
    const char* key = "\"id\"";
    const char* pos = strstr(json, key);
    if (!pos) return 0;
    
    pos += strlen(key);
    while (*pos && isspace((unsigned char)*pos)) pos++;
    if (*pos != ':') return 0;
    pos++;
    while (*pos && isspace((unsigned char)*pos)) pos++;
    
    if (*pos == '"') {
        pos++;
        const char* end = pos;
        while (*end && (*end != '"' || *(end-1) == '\\')) end++;
        size_t len = end - pos;
        msg->id = (char*)malloc(len + 1);
        if (msg->id) {
            strncpy(msg->id, pos, len);
            msg->id[len] = '\0';
        }
        msg->id_is_number = 0;
        return 1;
    } else if (isdigit((unsigned char)*pos) || *pos == '-') {
        msg->id_number = atoi(pos);
        msg->id_is_number = 1;
        return 1;
    }
    return 0;
}

static char* extract_json_string(const char* json, const char* key, int* end_pos) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* pos = strstr(json, pattern);
    if (!pos) return NULL;
    
    pos += strlen(pattern);
    while (*pos && isspace((unsigned char)*pos)) pos++;
    if (*pos != ':') return NULL;
    pos++;
    while (*pos && isspace((unsigned char)*pos)) pos++;
    
    if (*pos == '"') {
        pos++;
        const char* end = pos;
        while (*end && (*end != '"' || *(end-1) == '\\')) end++;
        size_t len = end - pos;
        char* result = (char*)malloc(len + 1);
        if (result) {
            strncpy(result, pos, len);
            result[len] = '\0';
            if (end_pos) *end_pos = (int)(end - json);
        }
        return result;
    } else if (*pos == '{') {
        int brace_count = 1;
        const char* end = pos + 1;
        while (*end && brace_count > 0) {
            if (*end == '{') brace_count++;
            else if (*end == '}') brace_count--;
            end++;
        }
        size_t len = end - pos;
        char* result = (char*)malloc(len + 1);
        if (result) {
            strncpy(result, pos, len);
            result[len] = '\0';
            if (end_pos) *end_pos = (int)(end - json);
        }
        return result;
    } else if (*pos == '[') {
        int bracket_count = 1;
        const char* end = pos + 1;
        while (*end && bracket_count > 0) {
            if (*end == '[') bracket_count++;
            else if (*end == ']') bracket_count--;
            end++;
        }
        size_t len = end - pos;
        char* result = (char*)malloc(len + 1);
        if (result) {
            strncpy(result, pos, len);
            result[len] = '\0';
            if (end_pos) *end_pos = (int)(end - json);
        }
        return result;
    } else {
        const char* end = pos;
        while (*end && (*end != ',' && *end != '}' && !isspace((unsigned char)*end))) end++;
        size_t len = end - pos;
        char* result = (char*)malloc(len + 1);
        if (result) {
            strncpy(result, pos, len);
            result[len] = '\0';
            if (end_pos) *end_pos = (int)(end - json);
        }
        return result;
    }
}

LspMessage* lsp_message_deserialize(const char* json) {
    if (!json) return NULL;
    
    LspMessage* msg = lsp_message_create();
    if (!msg) return NULL;
    
    msg->jsonrpc = extract_json_string(json, "jsonrpc", NULL);
    extract_id(json, msg);
    msg->method = extract_json_string(json, "method", NULL);
    msg->params = extract_json_string(json, "params", NULL);
    msg->result = extract_json_string(json, "result", NULL);
    
    char* error_obj = extract_json_string(json, "error", NULL);
    if (error_obj) {
        msg->error = error_obj;
        char* code_str = extract_json_string(error_obj, "code", NULL);
        if (code_str) {
            msg->error_code = atoi(code_str);
            free(code_str);
        }
    }
    
    return msg;
}

int lsp_message_is_request(const LspMessage* msg) {
    return msg && (msg->id || msg->id_is_number) && msg->method;
}

int lsp_message_is_response(const LspMessage* msg) {
    return msg && (msg->id || msg->id_is_number) && msg->result;
}

int lsp_message_is_notification(const LspMessage* msg) {
    return msg && !msg->id && !msg->id_is_number && msg->method;
}

int lsp_message_is_batch(const char* json) {
    if (!json) return 0;
    while (*json && isspace((unsigned char)*json)) json++;
    return *json == '[';
}

LspMessageBatch* lsp_message_batch_parse(const char* json) {
    if (!json || !lsp_message_is_batch(json)) return NULL;
    
    LspMessageBatch* batch = (LspMessageBatch*)malloc(sizeof(LspMessageBatch));
    if (!batch) return NULL;
    batch->items = NULL;
    batch->count = 0;
    batch->capacity = 0;
    
    return batch;
}

void lsp_message_batch_destroy(LspMessageBatch* batch) {
    if (!batch) return;
    for (int i = 0; i < batch->count; i++) {
        free(batch->items[i]);
    }
    free(batch->items);
    free(batch);
}

char* lsp_json_escape(const char* str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    size_t escaped_len = len + 1;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                escaped_len++;
                break;
            default:
                if ((unsigned char)str[i] < 0x20) {
                    escaped_len += 5;
                }
                break;
        }
    }
    
    char* result = (char*)malloc(escaped_len);
    if (!result) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"': result[j++] = '\\'; result[j++] = '"'; break;
            case '\\': result[j++] = '\\'; result[j++] = '\\'; break;
            case '\b': result[j++] = '\\'; result[j++] = 'b'; break;
            case '\f': result[j++] = '\\'; result[j++] = 'f'; break;
            case '\n': result[j++] = '\\'; result[j++] = 'n'; break;
            case '\r': result[j++] = '\\'; result[j++] = 'r'; break;
            case '\t': result[j++] = '\\'; result[j++] = 't'; break;
            default:
                if ((unsigned char)str[i] < 0x20) {
                    sprintf(result + j, "\\u%04x", (unsigned char)str[i]);
                    j += 6;
                } else {
                    result[j++] = str[i];
                }
                break;
        }
    }
    result[j] = '\0';
    return result;
}

char* lsp_json_unescape(const char* str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len) {
            switch (str[i + 1]) {
                case '"': result[j++] = '"'; i++; break;
                case '\\': result[j++] = '\\'; i++; break;
                case 'b': result[j++] = '\b'; i++; break;
                case 'f': result[j++] = '\f'; i++; break;
                case 'n': result[j++] = '\n'; i++; break;
                case 'r': result[j++] = '\r'; i++; break;
                case 't': result[j++] = '\t'; i++; break;
                case 'u':
                    if (i + 5 < len) {
                        char hex[5] = {str[i+2], str[i+3], str[i+4], str[i+5], '\0'};
                        unsigned int code = (unsigned int)strtol(hex, NULL, 16);
                        if (code < 0x80) {
                            result[j++] = (char)code;
                        }
                        i += 5;
                    }
                    break;
                default:
                    result[j++] = str[i];
                    break;
            }
        } else {
            result[j++] = str[i];
        }
    }
    result[j] = '\0';
    return result;
}

char* lsp_read_header_content_length(const char* header) {
    if (!header) return NULL;
    const char* prefix = "Content-Length: ";
    const char* pos = strstr(header, prefix);
    if (!pos) return NULL;
    pos += strlen(prefix);
    const char* end = strstr(pos, "\r\n");
    if (!end) end = pos + strlen(pos);
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    if (result) {
        strncpy(result, pos, len);
        result[len] = '\0';
    }
    return result;
}

int lsp_write_message(FILE* out, const char* content) {
    if (!out || !content) return -1;
    
    size_t len = strlen(content);
    fprintf(out, "Content-Length: %zu\r\n", len);
    fprintf(out, "\r\n");
    fprintf(out, "%s", content);
    fflush(out);
    return 0;
}

char* lsp_read_message(FILE* in) {
    if (!in) return NULL;
    
    char header[4096];
    size_t header_len = 0;
    int in_header = 1;
    int c;
    
    while (in_header && header_len < sizeof(header) - 1) {
        c = fgetc(in);
        if (c == EOF) return NULL;
        header[header_len++] = (char)c;
        header[header_len] = '\0';
        
        if (header_len >= 4 && strcmp(header + header_len - 4, "\r\n\r\n") == 0) {
            in_header = 0;
        }
    }
    
    char* len_str = lsp_read_header_content_length(header);
    if (!len_str) return NULL;
    
    size_t content_len = atoi(len_str);
    free(len_str);
    
    if (content_len == 0 || content_len > 100 * 1024 * 1024) {
        return NULL;
    }
    
    char* content = (char*)malloc(content_len + 1);
    if (!content) return NULL;
    
    size_t read_len = fread(content, 1, content_len, in);
    if (read_len != content_len) {
        free(content);
        return NULL;
    }
    content[content_len] = '\0';
    
    return content;
}