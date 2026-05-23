#ifndef LSP_LOG_H
#define LSP_LOG_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LSP_LOG_LEVEL_TRACE,
    LSP_LOG_LEVEL_DEBUG,
    LSP_LOG_LEVEL_INFO,
    LSP_LOG_LEVEL_WARN,
    LSP_LOG_LEVEL_ERROR,
    LSP_LOG_LEVEL_FATAL
} LspLogLevel;

void lsp_log_init(const char* filename, LspLogLevel min_level);
void lsp_log_close(void);
void lsp_log(LspLogLevel level, const char* file, int line, const char* fmt, ...);
void lsp_log_set_level(LspLogLevel level);
int lsp_log_is_enabled(LspLogLevel level);

#define LSP_LOG_TRACE(fmt, ...) lsp_log(LSP_LOG_LEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LSP_LOG_DEBUG(fmt, ...) lsp_log(LSP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LSP_LOG_INFO(fmt, ...)  lsp_log(LSP_LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LSP_LOG_WARN(fmt, ...)  lsp_log(LSP_LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LSP_LOG_ERROR(fmt, ...) lsp_log(LSP_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LSP_LOG_FATAL(fmt, ...) lsp_log(LSP_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif