#ifndef LSP_FEATURES_DIAGNOSTICS_H
#define LSP_FEATURES_DIAGNOSTICS_H

#include "../protocol/lsp_messages.h"

LspDiagnosticList* lsp_analyze_document(const char* uri, const char* content);
LspCompletionList* lsp_get_completions(const char* content, int line, int col);
char* lsp_get_hover_info(const char* content, int line, int col);
LspLocation* lsp_get_definition(const char* content, int line, int col);

#endif