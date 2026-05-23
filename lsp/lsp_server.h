#ifndef LSP_SERVER_H
#define LSP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol/json_rpc.h"
#include "protocol/lsp_messages.h"
#include "lsp_document.h"

typedef enum {
    LSP_SERVER_STATE_UNINITIALIZED,
    LSP_SERVER_STATE_INITIALIZING,
    LSP_SERVER_STATE_INITIALIZED,
    LSP_SERVER_STATE_SHUTDOWN
} LspServerState;

typedef struct {
    char* name;
    char* version;
    char* root_uri;
    int process_id;
    LspServerState state;
    int trace;
    
    LspDocumentStore* documents;
    
    int capabilities_text_document_sync;
    int capabilities_completion_provider;
    int capabilities_hover_provider;
    int capabilities_definition_provider;
    int capabilities_diagnostics;
    
    FILE* input;
    FILE* output;
    int running;
    int exit_code;
} LspServer;

typedef LspMessage* (*LspMethodHandler)(LspServer* server, const char* id, const char* params);

typedef struct {
    char* method;
    LspMethodHandler handler;
} LspMethodEntry;

typedef struct {
    LspMethodEntry* entries;
    int count;
    int capacity;
} LspMethodRegistry;

LspServer* lsp_server_create(void);
void lsp_server_destroy(LspServer* server);

int lsp_server_initialize(LspServer* server, const char* root_uri, int process_id);
void lsp_server_shutdown(LspServer* server);
void lsp_server_exit(LspServer* server);

void lsp_server_set_io(LspServer* server, FILE* input, FILE* output);
int lsp_server_run(LspServer* server);
void lsp_server_stop(LspServer* server);

LspMessage* lsp_server_handle_message(LspServer* server, const LspMessage* msg);

LspMethodRegistry* lsp_method_registry_create(void);
void lsp_method_registry_destroy(LspMethodRegistry* registry);
void lsp_method_registry_register(LspMethodRegistry* registry, const char* method, LspMethodHandler handler);
LspMethodHandler lsp_method_registry_find(LspMethodRegistry* registry, const char* method);

char* lsp_server_get_capabilities_json(LspServer* server);

void lsp_send_notification(LspServer* server, const char* method, const char* params);
void lsp_publish_diagnostics(LspServer* server, const char* uri, const LspDiagnosticList* diagnostics);

#endif