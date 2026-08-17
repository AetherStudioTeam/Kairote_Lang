#ifndef OUTPUT_CACHE_H
#define OUTPUT_CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum {
    OUTPUT_CACHE_STDOUT = 0,
    OUTPUT_CACHE_STDERR = 1
} OutputCacheStream;

typedef struct OutputCacheEntry {
    char* message;
    OutputCacheStream stream;
    struct OutputCacheEntry* next;
} OutputCacheEntry;

typedef struct {
    OutputCacheEntry* head;
    OutputCacheEntry* tail;
    int count;
    int enabled;
} OutputCache;

extern OutputCache g_output_cache;

void KrtOutputCacheInit(void);
void KrtOutputCacheCleanup(void);
void KrtOutputCacheClear(void);
void KrtOutputCacheAdd(const char* format, ...);
void KrtOutputCacheAddv(const char* format, va_list args);
void KrtOutputCacheAddError(const char* format, ...);
void KrtOutputCacheAddErrorv(const char* format, va_list args);
void KrtOutputCacheFlush(void);
int KrtOutputCacheIsEnabled(void);
void KrtOutputCacheSetEnabled(int enabled);

int KrtPrintf(const char* format, ...);
int KrtPrintFormat(const char* format, ...);
int KrtFprintf(FILE* stream, const char* format, ...);

#endif