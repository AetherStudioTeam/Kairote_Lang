#ifndef LSP_MESSAGES_H
#define LSP_MESSAGES_H

#include "json_rpc.h"

#define LSP_PROTOCOL_VERSION "3.17.0"

typedef struct {
    int line;
    int character;
} LspPosition;

typedef struct {
    LspPosition start;
    LspPosition end;
} LspRange;

typedef struct {
    char* uri;
    LspRange range;
} LspLocation;

typedef struct {
    int start_line;
    int start_char;
    int end_line;
    int end_char;
    int severity;
    char* code;
    char* source;
    char* message;
} LspDiagnostic;

typedef struct {
    LspDiagnostic* items;
    int count;
    int capacity;
} LspDiagnosticList;

typedef enum {
    LSP_SEVERITY_ERROR = 1,
    LSP_SEVERITY_WARNING = 2,
    LSP_SEVERITY_INFORMATION = 3,
    LSP_SEVERITY_HINT = 4
} LspDiagnosticSeverity;

typedef struct {
    char* jsonrpc;
    int process_id;
    char* root_uri;
    char* root_path;
    char* locale;
    char* trace;
} LspInitializeParams;

typedef struct {
    char* name;
    char* version;
} LspServerInfo;

typedef struct {
    char* capabilities_json;
    LspServerInfo server_info;
} LspInitializeResult;

typedef struct {
    char* uri;
    char* language_id;
    int version;
    char* text;
} LspTextDocumentItem;

typedef struct {
    char* uri;
    int version;
    char* text;
} LspTextDocumentContentChangeEvent;

typedef struct {
    char* value;
} LspChangeAnnotation;

typedef struct {
    LspRange range;
    char* new_text;
    char* annotation_id;
} LspTextEdit;

typedef struct {
    char* title;
    char* kind;
    char* diagnostics_json;
    char* edit_json;
    char* command_json;
    int is_preferred;
    int disabled;
    char* disabled_reason;
} LspCodeAction;

typedef struct {
    char* label;
    int kind;
    LspTextEdit* edits;
    int edit_count;
    char* documentation;
    char* detail;
    char* sort_text;
    char* filter_text;
    char* insert_text;
    int insert_text_format;
    char* insert_text_mode;
    char* text_edit_json;
    char* additional_text_edits_json;
    char* commit_characters;
    char* command_json;
    char* data_json;
} LspCompletionItem;

typedef struct {
    LspCompletionItem* items;
    int count;
    int capacity;
    int is_incomplete;
} LspCompletionList;

typedef enum {
    LSP_COMPLETION_ITEM_KIND_TEXT = 1,
    LSP_COMPLETION_ITEM_KIND_METHOD = 2,
    LSP_COMPLETION_ITEM_KIND_FUNCTION = 3,
    LSP_COMPLETION_ITEM_KIND_CONSTRUCTOR = 4,
    LSP_COMPLETION_ITEM_KIND_FIELD = 5,
    LSP_COMPLETION_ITEM_KIND_VARIABLE = 6,
    LSP_COMPLETION_ITEM_KIND_CLASS = 7,
    LSP_COMPLETION_ITEM_KIND_INTERFACE = 8,
    LSP_COMPLETION_ITEM_KIND_MODULE = 9,
    LSP_COMPLETION_ITEM_KIND_PROPERTY = 10,
    LSP_COMPLETION_ITEM_KIND_UNIT = 11,
    LSP_COMPLETION_ITEM_KIND_VALUE = 12,
    LSP_COMPLETION_ITEM_KIND_ENUM = 13,
    LSP_COMPLETION_ITEM_KIND_KEYWORD = 14,
    LSP_COMPLETION_ITEM_KIND_SNIPPET = 15,
    LSP_COMPLETION_ITEM_KIND_COLOR = 16,
    LSP_COMPLETION_ITEM_KIND_FILE = 17,
    LSP_COMPLETION_ITEM_KIND_REFERENCE = 18,
    LSP_COMPLETION_ITEM_KIND_FOLDER = 19,
    LSP_COMPLETION_ITEM_KIND_ENUM_MEMBER = 20,
    LSP_COMPLETION_ITEM_KIND_CONSTANT = 21,
    LSP_COMPLETION_ITEM_KIND_STRUCT = 22,
    LSP_COMPLETION_ITEM_KIND_EVENT = 23,
    LSP_COMPLETION_ITEM_KIND_OPERATOR = 24,
    LSP_COMPLETION_ITEM_KIND_TYPE_PARAMETER = 25
} LspCompletionItemKind;

typedef enum {
    LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT = 1,
    LSP_INSERT_TEXT_FORMAT_SNIPPET = 2
} LspInsertTextFormat;

typedef enum {
    LSP_TEXT_DOCUMENT_SYNC_NONE = 0,
    LSP_TEXT_DOCUMENT_SYNC_FULL = 1,
    LSP_TEXT_DOCUMENT_SYNC_INCREMENTAL = 2
} LspTextDocumentSyncKind;

char* lsp_position_to_json(const LspPosition* pos);
char* lsp_range_to_json(const LspRange* range);
char* lsp_location_to_json(const LspLocation* loc);
char* lsp_diagnostic_to_json(const LspDiagnostic* diag);
char* lsp_diagnostic_list_to_json(const LspDiagnosticList* list);
char* lsp_completion_item_to_json(const LspCompletionItem* item);
char* lsp_completion_list_to_json(const LspCompletionList* list);

LspPosition* lsp_position_from_json(const char* json);
LspRange* lsp_range_from_json(const char* json);
LspLocation* lsp_location_from_json(const char* json);
LspDiagnostic* lsp_diagnostic_from_json(const char* json);

void lsp_position_destroy(LspPosition* pos);
void lsp_range_destroy(LspRange* range);
void lsp_location_destroy(LspLocation* loc);
void lsp_diagnostic_destroy(LspDiagnostic* diag);
void lsp_diagnostic_list_destroy(LspDiagnosticList* list);
void lsp_completion_item_destroy(LspCompletionItem* item);
void lsp_completion_list_destroy(LspCompletionList* list);

LspDiagnosticList* lsp_diagnostic_list_create(void);
void lsp_diagnostic_list_add(LspDiagnosticList* list, const LspDiagnostic* diag);

LspCompletionList* lsp_completion_list_create(void);
void lsp_completion_list_add(LspCompletionList* list, const LspCompletionItem* item);

#endif