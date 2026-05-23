#ifndef LSP_DOCUMENT_H
#define LSP_DOCUMENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* uri;
    char* content;
    int version;
    char* language_id;
} LspDocument;

typedef struct {
    LspDocument** items;
    int count;
    int capacity;
} LspDocumentStore;

typedef struct {
    int start_line;
    int start_char;
    int end_line;
    int end_char;
    char* text;
} LspTextChange;

LspDocumentStore* lsp_document_store_create(void);
void lsp_document_store_destroy(LspDocumentStore* store);

LspDocument* lsp_document_create(const char* uri, const char* language_id, int version, const char* content);
void lsp_document_destroy(LspDocument* doc);

int lsp_document_store_open(LspDocumentStore* store, const char* uri, const char* language_id, int version, const char* content);
int lsp_document_store_change(LspDocumentStore* store, const char* uri, int version, const char* new_content);
int lsp_document_store_change_incremental(LspDocumentStore* store, const char* uri, int version, 
                                          const LspTextChange* changes, int change_count);
int lsp_document_store_close(LspDocumentStore* store, const char* uri);

LspDocument* lsp_document_store_get(LspDocumentStore* store, const char* uri);
char* lsp_document_store_get_content(LspDocumentStore* store, const char* uri);
int lsp_document_store_get_version(LspDocumentStore* store, const char* uri);

void lsp_document_get_line_col(const LspDocument* doc, int offset, int* line, int* col);
int lsp_document_offset_from_line_col(const LspDocument* doc, int line, int col);

#endif