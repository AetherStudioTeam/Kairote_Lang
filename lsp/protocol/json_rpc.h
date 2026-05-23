#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_RPC_VERSION "2.0"

typedef enum {
    LSP_PARSE_ERROR = -32700,
    LSP_INVALID_REQUEST = -32600,
    LSP_METHOD_NOT_FOUND = -32601,
    LSP_INVALID_PARAMS = -32602,
    LSP_INTERNAL_ERROR = -32603,
    LSP_SERVER_ERROR_START = -32099,
    LSP_SERVER_ERROR_END = -32000,
    LSP_SERVER_NOT_INITIALIZED = -32002,
    LSP_UNKNOWN_ERROR_CODE = -32001
} LspErrorCode;

typedef struct {
    char* jsonrpc;
    char* method;
    char* id;
    int id_is_number;
    int id_number;
    char* params;
    char* result;
    char* error;
    int error_code;
} LspMessage;

typedef struct {
    char** items;
    int count;
    int capacity;
} LspMessageBatch;

LspMessage* lsp_message_create(void);
void lsp_message_destroy(LspMessage* msg);

LspMessage* lsp_message_create_request(const char* id, const char* method, const char* params);
LspMessage* lsp_message_create_request_num(int id, const char* method, const char* params);
LspMessage* lsp_message_create_response(const char* id, const char* result);
LspMessage* lsp_message_create_response_num(int id, const char* result);
LspMessage* lsp_message_create_error(const char* id, int code, const char* message);
LspMessage* lsp_message_create_error_num(int id, int code, const char* message);
LspMessage* lsp_message_create_notification(const char* method, const char* params);

char* lsp_message_serialize(const LspMessage* msg);
LspMessage* lsp_message_deserialize(const char* json);

int lsp_message_is_request(const LspMessage* msg);
int lsp_message_is_response(const LspMessage* msg);
int lsp_message_is_notification(const LspMessage* msg);
int lsp_message_is_batch(const char* json);

LspMessageBatch* lsp_message_batch_parse(const char* json);
void lsp_message_batch_destroy(LspMessageBatch* batch);

char* lsp_json_escape(const char* str);
char* lsp_json_unescape(const char* str);

char* lsp_read_header_content_length(const char* header);
int lsp_write_message(FILE* out, const char* content);
char* lsp_read_message(FILE* in);

#endif