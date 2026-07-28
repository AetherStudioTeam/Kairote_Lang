#ifndef KRT_STANDALONE_H
#define KRT_STANDALONE_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void* KrtMemcpy(void* dest, const void* src, size_t n);
void* KrtMemset(void* s, int c, size_t n);
void* KrtMemmove(void* dest, const void* src, size_t n);
int KrtMemcmp(const void* s1, const void* s2, size_t n);
void* KrtMemchr(const void* s, int c, size_t n);

size_t KrtStrlen(const char* s);
char* KrtStrcpy(char* dest, const char* src);
char* KrtStrncpy(char* dest, const char* src, size_t n);
int KrtStrcmp(const char* s1, const char* s2);
int KrtStrncmp(const char* s1, const char* s2, size_t n);
char* KrtStrcat(char* dest, const char* src);
char* KrtStrncat(char* dest, const char* src, size_t n);
char* KrtStrchr(const char* s, int c);
char* KrtStrrchr(const char* s, int c);
char* KrtStrstr(const char* haystack, const char* needle);

void* KrtMalloc(size_t size);
void* KrtCalloc(size_t nmemb, size_t size);
void* KrtRealloc(void* ptr, size_t size);
void KrtFree(void* ptr);

typedef struct KrtMempool KrtMempoolT;
KrtMempoolT* KrtMempoolCreate(size_t block_size, size_t block_count);
void KrtMempoolDestroy(KrtMempoolT* pool);
void* KrtMempoolAlloc(KrtMempoolT* pool);
void KrtMempoolFree(KrtMempoolT* pool, void* ptr);

int KrtPrintf(const char* format, ...);
int KrtSprintf(char* str, const char* format, ...);
int KrtSnprintf(char* str, size_t size, const char* format, ...);
int KrtVprintf(const char* format, va_list ap);
int KrtVsprintf(char* str, const char* format, va_list ap);
int KrtVsnprintf(char* str, size_t size, const char* format, va_list ap);

int KrtPutchar(int c);
int KrtPuts(const char* s);

int KrtAtoi(const char* nptr);
long KrtAtol(const char* nptr);
long long KrtAtoll(const char* nptr);
long KrtStrtol(const char* nptr, char** endptr, int base);
unsigned long KrtStrtoul(const char* nptr, char** endptr, int base);

int KrtRand(void);
void KrtSrand(unsigned int seed);

void KrtQsort(void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*));
void* KrtBsearch(const void* key, const void* base, size_t nmemb, size_t size,
                 int (*compar)(const void*, const void*));

typedef long KrtClockT;
typedef long long KrtTimeT;

KrtClockT KrtClock(void);
KrtTimeT KrtTime(KrtTimeT* tloc);

void KrtExit(int status);
void KrtAbort(void);

int KrtAtexit(void (*func)(void));
void KrtCallAtexitHandlers(void);

int KrtGetargc(void);
char** KrtGetargv(void);

char* KrtGetenv(const char* name);

#ifdef _WIN32
void __cdecl mainCRTStartup(void);
void __cdecl WinMainCRTStartup(void);
#else
void _start(void);
#endif

#ifdef _WIN32

void* KrtWinHeapCreate(void);
void KrtWinHeapDestroy(void* heap);
void* KrtWinHeapAlloc(void* heap, size_t size);
void KrtWinHeapFree(void* heap, void* ptr);
void* KrtWinHeapRealloc(void* heap, void* ptr, size_t size);

void* KrtWinGetStdout(void);
void* KrtWinGetStderr(void);
int KrtWinWriteConsole(void* handle, const void* buffer, size_t size);
int KrtWinWriteFile(void* handle, const void* buffer, size_t size);
#endif

#endif 