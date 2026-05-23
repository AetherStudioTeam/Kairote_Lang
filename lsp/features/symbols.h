#ifndef LSP_FEATURES_SYMBOLS_H
#define LSP_FEATURES_SYMBOLS_H

#include "../protocol/lsp_messages.h"

typedef struct {
    char* name;
    char* uri;
    int line;
    int character;
    int kind;
    char* detail;
    char* documentation;
} LspSymbolInfo;

typedef struct {
    LspSymbolInfo* items;
    int count;
    int capacity;
} LspSymbolList;

LspSymbolList* lsp_symbol_list_create(void);
void lsp_symbol_list_destroy(LspSymbolList* list);
void lsp_symbol_list_add(LspSymbolList* list, const LspSymbolInfo* symbol);

LspSymbolList* lsp_document_symbols(const char* content, const char* uri);
LspSymbolList* lsp_workspace_symbols(const char* query);
LspSymbolInfo* lsp_find_symbol_at_position(const char* content, int line, int col);
LspSymbolInfo* lsp_find_symbol_definition(const char* content, const char* symbol_name);

void lsp_symbol_info_destroy(LspSymbolInfo* info);

char* lsp_symbol_kind_to_string(int kind);
int lsp_symbol_type_to_kind(const char* type);

#endif