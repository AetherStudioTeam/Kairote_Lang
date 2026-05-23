#ifndef LSP_HANDLERS_TEXT_DOCUMENT_H
#define LSP_HANDLERS_TEXT_DOCUMENT_H

#include "../lsp_server.h"

LspMessage* lsp_handle_text_document_did_open(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_did_change(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_did_close(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_completion(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_hover(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_definition(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_document_symbol(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_signature_help(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_formatting(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_range_formatting(LspServer* server, const char* id, const char* params);
LspMessage* lsp_handle_text_document_on_type_formatting(LspServer* server, const char* id, const char* params);

#endif