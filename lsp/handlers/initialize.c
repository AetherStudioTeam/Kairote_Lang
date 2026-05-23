#include "initialize.h"
#include <stdio.h>
#include <string.h>
#include "../lsp_log.h"

LspMessage* lsp_handle_initialize(LspServer* server, const char* id, const char* params) {
    if (!server) {
        return lsp_message_create_error(id, LSP_INTERNAL_ERROR, "Server is null");
    }
    
    if (server->state != LSP_SERVER_STATE_UNINITIALIZED) {
        return lsp_message_create_error(id, LSP_INVALID_REQUEST, "Server already initialized");
    }
    
    int process_id = -1;
    char* root_uri = NULL;
    
    if (params) {
        const char* pid_str = strstr(params, "\"processId\":");
        if (pid_str) {
            process_id = atoi(pid_str + 12);
        }
        
        const char* root_uri_str = strstr(params, "\"rootUri\":");
        if (root_uri_str) {
            root_uri_str += 10;
            while (*root_uri_str && (*root_uri_str == ' ' || *root_uri_str == '"')) root_uri_str++;
            const char* end = root_uri_str;
            while (*end && *end != '"' && *end != ',' && *end != '}') end++;
            if (end > root_uri_str) {
                size_t len = end - root_uri_str;
                root_uri = (char*)malloc(len + 1);
                if (root_uri) {
                    strncpy(root_uri, root_uri_str, len);
                    root_uri[len] = '\0';
                }
            }
        }
    }
    
    LSP_LOG_INFO("Initialize request: processId=%d, rootUri=%s", process_id, root_uri ? root_uri : "null");
    
    lsp_server_initialize(server, root_uri, process_id);
    free(root_uri);
    
    char* capabilities = lsp_server_get_capabilities_json(server);
    
    char result[8192];
    snprintf(result, sizeof(result),
        "{\"capabilities\":%s,\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
        capabilities, server->name, server->version);
    
    free(capabilities);
    
    server->state = LSP_SERVER_STATE_INITIALIZED;
    
    LSP_LOG_INFO("Server initialized successfully");
    
    return lsp_message_create_response(id, result);
}

LspMessage* lsp_handle_shutdown(LspServer* server, const char* id, const char* params) {
    if (!server) {
        return lsp_message_create_error(id, LSP_INTERNAL_ERROR, "Server is null");
    }
    
    if (server->state != LSP_SERVER_STATE_INITIALIZED) {
        return lsp_message_create_error(id, LSP_INVALID_REQUEST, "Server not initialized");
    }
    
    LSP_LOG_INFO("Shutdown request received");
    
    lsp_server_shutdown(server);
    return lsp_message_create_response(id, "null");
}

LspMessage* lsp_handle_exit(LspServer* server, const char* id, const char* params) {
    if (!server) {
        return NULL;
    }
    
    LSP_LOG_INFO("Exit notification received");
    
    lsp_server_exit(server);
    return NULL;
}