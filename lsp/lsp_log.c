#include "lsp_log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE* g_log_file = NULL;
static LspLogLevel g_min_level = LSP_LOG_LEVEL_INFO;
static int g_initialized = 0;

static const char* level_to_string(LspLogLevel level) {
    switch (level) {
        case LSP_LOG_LEVEL_TRACE: return "TRACE";
        case LSP_LOG_LEVEL_DEBUG: return "DEBUG";
        case LSP_LOG_LEVEL_INFO:  return "INFO";
        case LSP_LOG_LEVEL_WARN:  return "WARN";
        case LSP_LOG_LEVEL_ERROR: return "ERROR";
        case LSP_LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

void lsp_log_init(const char* filename, LspLogLevel min_level) {
    if (g_initialized) return;
    
    g_min_level = min_level;
    
    if (filename) {
        g_log_file = fopen(filename, "w");
    }
    
    g_initialized = 1;
    
    LSP_LOG_INFO("LSP Log initialized, level=%s", level_to_string(min_level));
}

void lsp_log_close(void) {
    if (!g_initialized) return;
    
    LSP_LOG_INFO("LSP Log closing");
    
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    g_initialized = 0;
}

void lsp_log_set_level(LspLogLevel level) {
    g_min_level = level;
}

int lsp_log_is_enabled(LspLogLevel level) {
    return g_initialized && level >= g_min_level;
}

void lsp_log(LspLogLevel level, const char* file, int line, const char* fmt, ...) {
    if (!g_initialized || level < g_min_level) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    const char* level_str = level_to_string(level);
    
    const char* filename = strrchr(file, '/');
    if (!filename) filename = strrchr(file, '\\');
    if (filename) filename++;
    else filename = file;
    
    FILE* output = g_log_file ? g_log_file : stderr;
    
    fprintf(output, "[%s] [%s] [%s:%d] ", time_buf, level_str, filename, line);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(output, fmt, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}