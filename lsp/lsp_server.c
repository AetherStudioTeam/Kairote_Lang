#include "lsp_server.h"
#include <ctype.h>

static LspMethodRegistry* g_registry = NULL;

extern LspMessage* lsp_handle_initialize(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_shutdown(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_exit(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_did_open(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_did_change(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_did_close(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_completion(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_hover(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_definition(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_document_symbol(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_signature_help(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_formatting(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_range_formatting(LspServer* server, const char* id, const char* params);
extern LspMessage* lsp_handle_text_document_on_type_formatting(LspServer* server, const char* id, const char* params);

static void lsp_register_default_handlers(void) {
    if (g_registry) return;
    g_registry = lsp_method_registry_create();
    lsp_method_registry_register(g_registry, "initialize", lsp_handle_initialize);
    lsp_method_registry_register(g_registry, "shutdown", lsp_handle_shutdown);
    lsp_method_registry_register(g_registry, "exit", lsp_handle_exit);
    lsp_method_registry_register(g_registry, "textDocument/didOpen", lsp_handle_text_document_did_open);
    lsp_method_registry_register(g_registry, "textDocument/didChange", lsp_handle_text_document_did_change);
    lsp_method_registry_register(g_registry, "textDocument/didClose", lsp_handle_text_document_did_close);
    lsp_method_registry_register(g_registry, "textDocument/completion", lsp_handle_text_document_completion);
    lsp_method_registry_register(g_registry, "textDocument/hover", lsp_handle_text_document_hover);
    lsp_method_registry_register(g_registry, "textDocument/definition", lsp_handle_text_document_definition);
    lsp_method_registry_register(g_registry, "textDocument/documentSymbol", lsp_handle_text_document_document_symbol);
    lsp_method_registry_register(g_registry, "textDocument/signatureHelp", lsp_handle_text_document_signature_help);
    lsp_method_registry_register(g_registry, "textDocument/formatting", lsp_handle_text_document_formatting);
    lsp_method_registry_register(g_registry, "textDocument/rangeFormatting", lsp_handle_text_document_range_formatting);
    lsp_method_registry_register(g_registry, "textDocument/onTypeFormatting", lsp_handle_text_document_on_type_formatting);
}

LspServer* lsp_server_create(void) {
    lsp_register_default_handlers();
    
    LspServer* server = (LspServer*)calloc(1, sizeof(LspServer));
    if (!server) return NULL;
    
    server->name = strdup("Kairote Lang Language Server");
    server->version = strdup("0.1.0");
    server->state = LSP_SERVER_STATE_UNINITIALIZED;
    server->documents = lsp_document_store_create();
    server->input = stdin;
    server->output = stdout;
    server->running = 0;
    server->exit_code = 0;
    
    server->capabilities_text_document_sync = LSP_TEXT_DOCUMENT_SYNC_INCREMENTAL;
    server->capabilities_completion_provider = 1;
    server->capabilities_hover_provider = 1;
    server->capabilities_definition_provider = 1;
    server->capabilities_diagnostics = 1;
    
    return server;
}

void lsp_server_destroy(LspServer* server) {
    if (!server) return;
    free(server->name);
    free(server->version);
    free(server->root_uri);
    lsp_document_store_destroy(server->documents);
    free(server);
}

int lsp_server_initialize(LspServer* server, const char* root_uri, int process_id) {
    if (!server) return -1;
    if (server->state != LSP_SERVER_STATE_UNINITIALIZED) return -1;
    
    server->root_uri = root_uri ? strdup(root_uri) : NULL;
    server->process_id = process_id;
    server->state = LSP_SERVER_STATE_INITIALIZING;
    
    return 0;
}

void lsp_server_shutdown(LspServer* server) {
    if (!server) return;
    if (server->state != LSP_SERVER_STATE_INITIALIZED) return;
    server->state = LSP_SERVER_STATE_SHUTDOWN;
}

void lsp_server_exit(LspServer* server) {
    if (!server) return;
    server->running = 0;
    server->exit_code = (server->state == LSP_SERVER_STATE_SHUTDOWN) ? 0 : 1;
}

void lsp_server_set_io(LspServer* server, FILE* input, FILE* output) {
    if (!server) return;
    server->input = input ? input : stdin;
    server->output = output ? output : stdout;
}

int lsp_server_run(LspServer* server) {
    if (!server) return -1;
    server->running = 1;
    
    while (server->running) {
        char* content = lsp_read_message(server->input);
        if (!content) {
            server->running = 0;
            break;
        }
        
        LspMessage* msg = lsp_message_deserialize(content);
        free(content);
        
        if (!msg) continue;
        
        LspMessage* response = lsp_server_handle_message(server, msg);
        lsp_message_destroy(msg);
        
        if (response) {
            char* response_str = lsp_message_serialize(response);
            lsp_message_destroy(response);
            if (response_str) {
                lsp_write_message(server->output, response_str);
                free(response_str);
            }
        }
    }
    
    return server->exit_code;
}

void lsp_server_stop(LspServer* server) {
    if (!server) return;
    server->running = 0;
}

LspMessage* lsp_server_handle_message(LspServer* server, const LspMessage* msg) {
    if (!server || !msg) return NULL;
    
    if (lsp_message_is_notification(msg)) {
        LspMethodHandler handler = lsp_method_registry_find(g_registry, msg->method);
        if (handler) {
            handler(server, NULL, msg->params);
        }
        return NULL;
    }
    
    if (lsp_message_is_request(msg)) {
        if (server->state == LSP_SERVER_STATE_UNINITIALIZED && strcmp(msg->method, "initialize") != 0) {
            return lsp_message_create_error(msg->id, LSP_SERVER_NOT_INITIALIZED, "Server not initialized");
        }
        
        LspMethodHandler handler = lsp_method_registry_find(g_registry, msg->method);
        if (!handler) {
            return lsp_message_create_error(msg->id, LSP_METHOD_NOT_FOUND, "Method not found");
        }
        
        return handler(server, msg->id, msg->params);
    }
    
    return NULL;
}

LspMethodRegistry* lsp_method_registry_create(void) {
    LspMethodRegistry* registry = (LspMethodRegistry*)calloc(1, sizeof(LspMethodRegistry));
    if (!registry) return NULL;
    registry->capacity = 32;
    registry->entries = (LspMethodEntry*)calloc(registry->capacity, sizeof(LspMethodEntry));
    if (!registry->entries) {
        free(registry);
        return NULL;
    }
    return registry;
}

void lsp_method_registry_destroy(LspMethodRegistry* registry) {
    if (!registry) return;
    for (int i = 0; i < registry->count; i++) {
        free(registry->entries[i].method);
    }
    free(registry->entries);
    free(registry);
}

void lsp_method_registry_register(LspMethodRegistry* registry, const char* method, LspMethodHandler handler) {
    if (!registry || !method || !handler) return;
    
    if (registry->count >= registry->capacity) {
        registry->capacity *= 2;
        LspMethodEntry* new_entries = (LspMethodEntry*)realloc(registry->entries, registry->capacity * sizeof(LspMethodEntry));
        if (!new_entries) return;
        registry->entries = new_entries;
    }
    
    registry->entries[registry->count].method = strdup(method);
    registry->entries[registry->count].handler = handler;
    registry->count++;
}

LspMethodHandler lsp_method_registry_find(LspMethodRegistry* registry, const char* method) {
    if (!registry || !method) return NULL;
    
    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->entries[i].method, method) == 0) {
            return registry->entries[i].handler;
        }
    }
    return NULL;
}

char* lsp_server_get_capabilities_json(LspServer* server) {
    if (!server) return strdup("{}");
    
    char buffer[4096];
    int offset = 0;
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "{");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "\"textDocumentSync\":{\"openClose\":true,\"change\":%d,\"willSave\":false,\"willSaveWaitUntil\":false}",
        server->capabilities_text_document_sync);
    
    if (server->capabilities_completion_provider) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            ",\"completionProvider\":{\"triggerCharacters\":[\".\",\"::\"],\"resolveProvider\":false}");
    }
    
    if (server->capabilities_hover_provider) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            ",\"hoverProvider\":true");
    }
    
    if (server->capabilities_definition_provider) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            ",\"definitionProvider\":true");
    }
    
    if (server->capabilities_diagnostics) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            ",\"diagnosticProvider\":{\"interFileDependencies\":true,\"workspaceDiagnostics\":false}");
    }
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        ",\"documentSymbolProvider\":true");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        ",\"signatureHelpProvider\":{\"triggerCharacters\":[\"(\",\",\"]}");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        ",\"documentFormattingProvider\":true");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        ",\"documentRangeFormattingProvider\":true");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        ",\"documentOnTypeFormattingProvider\":{\"firstTriggerCharacter\":\";\",\"moreTriggerCharacter\":[\"}\"]}");
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "}");
    
    return strdup(buffer);
}

void lsp_send_notification(LspServer* server, const char* method, const char* params) {
    if (!server || !method) return;
    
    LspMessage* msg = lsp_message_create_notification(method, params);
    if (!msg) return;
    
    char* content = lsp_message_serialize(msg);
    lsp_message_destroy(msg);
    
    if (content) {
        lsp_write_message(server->output, content);
        free(content);
    }
}

void lsp_publish_diagnostics(LspServer* server, const char* uri, const LspDiagnosticList* diagnostics) {
    if (!server || !uri) return;
    
    char* diagnostics_json = lsp_diagnostic_list_to_json(diagnostics);
    if (!diagnostics_json) return;
    
    char params[8192];
    snprintf(params, sizeof(params), "{\"uri\":\"%s\",\"version\":%d,\"diagnostics\":%s}",
             uri, lsp_document_store_get_version(server->documents, uri), diagnostics_json);
    free(diagnostics_json);
    
    lsp_send_notification(server, "textDocument/publishDiagnostics", params);
}