
#include "KrtStandalone.h"

#ifndef _WIN32
#include <sys/syscall.h>
#endif

#ifdef _WIN32
#include <windows.h>

extern int main(int argc, char** argv);

static int g_argc = 0;
static char* g_argv[256];
static char g_cmdline_buffer[32768];

int KrtGetargc(void) {
    return g_argc;
}

char** KrtGetargv(void) {
    return g_argv;
}

static void parse_cmdline(void) __attribute__((unused));

static void parse_cmdline(void) {

    const char* cmdline = GetCommandLineA();
    if (!cmdline) {
        g_argc = 0;
        return;
    }
    
    size_t len = KrtStrlen(cmdline);
    if (len >= sizeof(g_cmdline_buffer)) {
        len = sizeof(g_cmdline_buffer) - 1;
    }
    KrtMemcpy(g_cmdline_buffer, cmdline, len);
    g_cmdline_buffer[len] = '\0';
    
    char* p = g_cmdline_buffer;
    g_argc = 0;
    
    while (*p && g_argc < 256) {
        
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        g_argv[g_argc++] = p;
        
        if (*p == '"') {
            p++;
            g_argv[g_argc - 1] = p;  
            
            while (*p && *p != '"') p++;
            
            if (*p == '"') {
                *p++ = '\0';  
            }
        } else {
            
            while (*p && *p != ' ' && *p != '\t') p++;
            
            if (*p) {
                *p++ = '\0';  
            }
        }
    }
}

void KrtExit(int status) {
    ExitProcess(status);
}

void KrtAbort(void) {
    ExitProcess(3);  
}

char* KrtGetenv(const char* name) {
    
    static char buffer[32768];
    DWORD len = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    
    if (len == 0 || len >= sizeof(buffer)) {
        return NULL;
    }
    
    return buffer;
}

#else

#include <unistd.h>
#include <sys/syscall.h>

extern int main(int argc, char** argv);

static int g_argc = 0;
static char** g_argv = NULL;

int KrtGetargc(void) {
    return g_argc;
}

char** KrtGetargv(void) {
    return g_argv;
}

void _start(void) {
    
    register long* rsp __asm__("rsp");
    
    g_argc = (int)rsp[0];
    g_argv = (char**)(rsp + 1);
    
    int ret = main(g_argc, g_argv);
    
    syscall(SYS_exit, ret);
    __builtin_unreachable();
}

void KrtExit(int status) {
    syscall(SYS_exit, status);
    __builtin_unreachable();
}

void KrtAbort(void) {
    syscall(SYS_exit, 3);
    __builtin_unreachable();
}

char* KrtGetenv(const char* name) {
    
    extern char** environ;
    
    size_t name_len = KrtStrlen(name);
    
    for (int i = 0; environ[i]; i++) {
        if (KrtStrncmp(environ[i], name, name_len) == 0 && 
            environ[i][name_len] == '=') {
            return environ[i] + name_len + 1;
        }
    }
    
    return NULL;
}

#endif

#define MAX_ATEXIT_HANDLERS 32

static void (*atexit_handlers[MAX_ATEXIT_HANDLERS])(void);
static int atexit_count = 0;

int KrtAtexit(void (*func)(void)) {
    if (atexit_count >= MAX_ATEXIT_HANDLERS) {
        return -1;
    }
    
    atexit_handlers[atexit_count++] = func;
    return 0;
}

void KrtCallAtexitHandlers(void) {
    
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_handlers[i]) {
            atexit_handlers[i]();
        }
    }
}