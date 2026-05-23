#include "lsp_document.h"

LspDocumentStore* lsp_document_store_create(void) {
    LspDocumentStore* store = (LspDocumentStore*)calloc(1, sizeof(LspDocumentStore));
    if (!store) return NULL;
    store->capacity = 16;
    store->items = (LspDocument**)calloc(store->capacity, sizeof(LspDocument*));
    if (!store->items) {
        free(store);
        return NULL;
    }
    return store;
}

void lsp_document_store_destroy(LspDocumentStore* store) {
    if (!store) return;
    for (int i = 0; i < store->count; i++) {
        lsp_document_destroy(store->items[i]);
    }
    free(store->items);
    free(store);
}

LspDocument* lsp_document_create(const char* uri, const char* language_id, int version, const char* content) {
    if (!uri) return NULL;
    LspDocument* doc = (LspDocument*)calloc(1, sizeof(LspDocument));
    if (!doc) return NULL;
    
    doc->uri = strdup(uri);
    doc->language_id = language_id ? strdup(language_id) : NULL;
    doc->version = version;
    doc->content = content ? strdup(content) : NULL;
    
    return doc;
}

void lsp_document_destroy(LspDocument* doc) {
    if (!doc) return;
    free(doc->uri);
    free(doc->language_id);
    free(doc->content);
    free(doc);
}

int lsp_document_store_open(LspDocumentStore* store, const char* uri, const char* language_id, int version, const char* content) {
    if (!store || !uri) return -1;
    
    LspDocument* existing = lsp_document_store_get(store, uri);
    if (existing) {
        free(existing->content);
        existing->content = content ? strdup(content) : NULL;
        existing->version = version;
        return 0;
    }
    
    if (store->count >= store->capacity) {
        store->capacity *= 2;
        LspDocument** new_items = (LspDocument**)realloc(store->items, store->capacity * sizeof(LspDocument*));
        if (!new_items) return -1;
        store->items = new_items;
    }
    
    store->items[store->count] = lsp_document_create(uri, language_id, version, content);
    if (!store->items[store->count]) return -1;
    store->count++;
    return 0;
}

int lsp_document_store_change(LspDocumentStore* store, const char* uri, int version, const char* new_content) {
    if (!store || !uri) return -1;
    
    LspDocument* doc = lsp_document_store_get(store, uri);
    if (!doc) return -1;
    
    free(doc->content);
    doc->content = new_content ? strdup(new_content) : NULL;
    doc->version = version;
    return 0;
}

static int offset_from_line_col_internal(const char* content, int line, int col) {
    if (!content) return 0;
    
    int current_line = 0;
    int current_col = 0;
    int offset = 0;
    
    for (offset = 0; content[offset]; offset++) {
        if (current_line == line && current_col == col) {
            return offset;
        }
        
        if (content[offset] == '\n') {
            if (current_line == line) {
                return offset;
            }
            current_line++;
            current_col = 0;
        } else {
            current_col++;
        }
    }
    
    return offset;
}

static char* apply_text_change(const char* content, const LspTextChange* change) {
    if (!content || !change) return NULL;
    
    int start_offset = offset_from_line_col_internal(content, change->start_line, change->start_char);
    int end_offset = offset_from_line_col_internal(content, change->end_line, change->end_char);
    
    if (start_offset < 0) start_offset = 0;
    if (end_offset < start_offset) end_offset = start_offset;
    if (end_offset > (int)strlen(content)) end_offset = (int)strlen(content);
    
    size_t prefix_len = start_offset;
    size_t suffix_len = strlen(content) - end_offset;
    size_t new_text_len = change->text ? strlen(change->text) : 0;
    
    size_t new_len = prefix_len + new_text_len + suffix_len;
    char* new_content = (char*)malloc(new_len + 1);
    if (!new_content) return NULL;
    
    memcpy(new_content, content, prefix_len);
    if (change->text) {
        memcpy(new_content + prefix_len, change->text, new_text_len);
    }
    memcpy(new_content + prefix_len + new_text_len, content + end_offset, suffix_len);
    new_content[new_len] = '\0';
    
    return new_content;
}

int lsp_document_store_change_incremental(LspDocumentStore* store, const char* uri, int version,
                                          const LspTextChange* changes, int change_count) {
    if (!store || !uri || !changes || change_count <= 0) return -1;
    
    LspDocument* doc = lsp_document_store_get(store, uri);
    if (!doc) return -1;
    
    char* current_content = strdup(doc->content ? doc->content : "");
    if (!current_content) return -1;
    
    for (int i = 0; i < change_count; i++) {
        char* new_content = apply_text_change(current_content, &changes[i]);
        if (!new_content) {
            free(current_content);
            return -1;
        }
        free(current_content);
        current_content = new_content;
    }
    
    free(doc->content);
    doc->content = current_content;
    doc->version = version;
    
    return 0;
}

int lsp_document_store_close(LspDocumentStore* store, const char* uri) {
    if (!store || !uri) return -1;
    
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->items[i]->uri, uri) == 0) {
            lsp_document_destroy(store->items[i]);
            for (int j = i; j < store->count - 1; j++) {
                store->items[j] = store->items[j + 1];
            }
            store->count--;
            return 0;
        }
    }
    return -1;
}

LspDocument* lsp_document_store_get(LspDocumentStore* store, const char* uri) {
    if (!store || !uri) return NULL;
    
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->items[i]->uri, uri) == 0) {
            return store->items[i];
        }
    }
    return NULL;
}

char* lsp_document_store_get_content(LspDocumentStore* store, const char* uri) {
    LspDocument* doc = lsp_document_store_get(store, uri);
    return doc ? doc->content : NULL;
}

int lsp_document_store_get_version(LspDocumentStore* store, const char* uri) {
    LspDocument* doc = lsp_document_store_get(store, uri);
    return doc ? doc->version : -1;
}

void lsp_document_get_line_col(const LspDocument* doc, int offset, int* line, int* col) {
    if (!doc || !line || !col) return;
    
    *line = 0;
    *col = 0;
    
    if (!doc->content) return;
    
    for (int i = 0; i < offset && doc->content[i]; i++) {
        if (doc->content[i] == '\n') {
            (*line)++;
            *col = 0;
        } else {
            (*col)++;
        }
    }
}

int lsp_document_offset_from_line_col(const LspDocument* doc, int line, int col) {
    if (!doc || !doc->content) return -1;
    
    return offset_from_line_col_internal(doc->content, line, col);
}