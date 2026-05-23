#ifndef LSP_FEATURES_SIGNATURE_H
#define LSP_FEATURES_SIGNATURE_H

#include "../protocol/lsp_messages.h"

typedef struct {
    char* label;
    char* documentation;
    LspParameterInfo* parameters;
    int parameter_count;
    int active_parameter;
} LspSignatureInfo;

typedef struct {
    char* label;
    char* documentation;
} LspParameterInfo;

typedef struct {
    LspSignatureInfo* signatures;
    int signature_count;
    int active_signature;
    int active_parameter;
} LspSignatureHelp;

LspSignatureHelp* lsp_get_signature_help(const char* content, int line, int col);
void lsp_signature_help_destroy(LspSignatureHelp* help);
void lsp_signature_info_destroy(LspSignatureInfo* sig);

char* lsp_signature_help_to_json(const LspSignatureHelp* help);

#endif