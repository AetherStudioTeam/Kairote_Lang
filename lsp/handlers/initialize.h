#ifndef LSP_HANDLERS_INITIALIZE_H
#define LSP_HANDLERS_INITIALIZE_H

#include "../lsp_server.h"

LspMessage* lsp_handle_initialize(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_shutdown(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_exit(LspServer* server, const char* id, const char* params);

#endif