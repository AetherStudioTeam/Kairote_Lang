#ifndef LSP_FEATURES_FORMATTING_H
#define LSP_FEATURES_FORMATTING_H

typedef struct {
    int tab_size;
    int use_spaces;
    int trim_trailing_whitespace;
    int insert_final_newline;
} LspFormatOptions;

char* lsp_format_document(const char* content, const LspFormatOptions* options);
char* lsp_format_range(const char* content, int start_line, int start_col, int end_line, int end_col, const LspFormatOptions* options);
char* lsp_format_on_type(const char* content, int line, int col, char ch, const LspFormatOptions* options);

#endif