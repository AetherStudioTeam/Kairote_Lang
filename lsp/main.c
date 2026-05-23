#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "lsp_server.h"
#include "lsp_log.h"

static void print_usage(const char* program) {
    fprintf(stderr, "Usage: %s [options]\n", program);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --stdio          Use stdio for communication (default)\n");
    fprintf(stderr, "  --log <file>     Log to file\n");
    fprintf(stderr, "  --version        Print version information\n");
    fprintf(stderr, "  --help           Print this help message\n");
}

static void print_version(void) {
    printf("Kairote Lang Language Server v0.1.0\n");
    printf("Protocol version: %s\n", LSP_PROTOCOL_VERSION);
    printf("JSON-RPC version: %s\n", JSON_RPC_VERSION);
}

int main(int argc, char** argv) {
    int use_stdio = 1;
    const char* log_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "--stdio") == 0) {
            use_stdio = 1;
        }
        if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_file = argv[++i];
        }
    }
    
    if (!use_stdio) {
        fprintf(stderr, "Only stdio transport is supported\n");
        return 1;
    }
    
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    
    lsp_log_init(log_file, LSP_LOG_LEVEL_DEBUG);
    
    LSP_LOG_INFO("Kairote Lang Language Server starting...");
    
    LspServer* server = lsp_server_create();
    if (!server) {
        LSP_LOG_FATAL("Failed to create LSP server");
        lsp_log_close();
        return 1;
    }
    
    lsp_server_set_io(server, stdin, stdout);
    
    int exit_code = lsp_server_run(server);
    
    lsp_server_destroy(server);
    lsp_log_close();
    
    return exit_code;
}