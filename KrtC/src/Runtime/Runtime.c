#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#if !defined(_WIN32) && !defined(__MINGW32__)
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "font_data.h"

extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "../Core/Platform/Platform.h"
#include "Runtime.h"
#include "../Core/Utils/KrtString.h"
#include "../Core/Utils/KrtCommon.h"

#if defined(_WIN32) || defined(__MINGW32__)
    #define KRT_RUNTIME_EXPORT __declspec(dllexport)
#else
    
    #define KRT_RUNTIME_EXPORT 
#endif

KRT_RUNTIME_EXPORT void KRT_API _print_string(const char* str) {
    if (!str) return;
    
#ifdef _WIN32
    
    static int console_initialized = 0;
    if (!console_initialized) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hStdOut, &mode)) {
            SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        
        console_initialized = 1;
    }
    
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    
    size_t len = 0;
    while (str[len]) len++;
    
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
    if (wlen > 0) {
        wchar_t* wstr = (wchar_t*)KRT_MALLOC(wlen * sizeof(wchar_t));
        if (wstr) {
            MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wstr, wlen);
            WriteConsoleW(hStdOut, wstr, wlen, &written, NULL);
            KRT_FREE(wstr);
            return;
        }
    }
    
    WriteConsole(hStdOut, str, (DWORD)len, &written, NULL);
#else
    size_t len = 0;
    while (str[len]) len++;
    size_t written;
    KrtWriteConsole(KRT_STDOUT_HANDLE, str, len, &written);
#endif
}

KRT_RUNTIME_EXPORT void KRT_API _print_number(int num) {
    char buffer[32];
    int i = 30;
    int negative = 0;
    
    if (num == INT32_MIN) {
        _print_string("-2147483648");
        return;
    }
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[31] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    _print_string(&buffer[i + 1]);
}

KRT_RUNTIME_EXPORT void KRT_API _print_int(int num) {
    _print_number(num);
}

KRT_RUNTIME_EXPORT void KRT_API _print_double(double num) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", num);
    _print_string(buffer);
}

KRT_RUNTIME_EXPORT void KRT_API _print_int64(long long num) {
    char buffer[32];
    int i = 30;
    int negative = 0;
    
    if (num == INT64_MIN) {
        _print_string("-9223372036854775808");
        return;
    }
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[31] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    _print_string(&buffer[i + 1]);
}

KRT_RUNTIME_EXPORT void KRT_API _print_float(double num) {
    
    if (isnan(num)) {
        _print_string("NaN");
        return;
    }
    
    if (isinf(num)) {
        if (num < 0) {
            _print_string("-Infinity");
        } else {
            _print_string("Infinity");
        }
        return;
    }
    
    if (num < 0) {
        _print_string("-");
        num = -num;
    }
    
    double int_part_d;
    if (modf(num, &int_part_d) < 1e-10) {
        _print_number((int)int_part_d);
        return;
    }
    
    int int_part = (int)num;
    _print_number(int_part);
    
    _print_string(".");
    num -= int_part;
    
    int precision = 6; 
    
    if (num < 0.001 && num > 0) {
        precision = 10;
    }
    
    if (int_part >= 1000000) {
        precision = 2;
    } else if (int_part >= 1000) {
        precision = 4;
    }
    
    for (int i = 0; i < precision; i++) {
        num *= 10;
        int digit = (int)num;
        _print_number(digit);
        num -= digit;
    }
    
}

KRT_RUNTIME_EXPORT void KRT_API _print_char(char c) {
#ifdef _WIN32
    DWORD written;
    KrtWriteConsole(KRT_STDOUT_HANDLE, &c, 1, &written);
#else
    size_t written;
    KrtWriteConsole(KRT_STDOUT_HANDLE, &c, 1, &written);
#endif
}

KRT_RUNTIME_EXPORT void KRT_API _print_newline(void) {
    _print_string("\n");
}

KRT_RUNTIME_EXPORT int KRT_API _read_char(void) {
#ifdef _WIN32
    char c;
    DWORD read;
    KrtReadConsole(KRT_STDIN_HANDLE, &c, 1, &read, NULL);
    return (read > 0) ? c : -1;
#else
    char c;
    ssize_t bytes_read_result;
    KrtReadConsole(KRT_STDIN_HANDLE, &c, 1, &bytes_read_result, NULL);
    return (bytes_read_result > 0) ? c : -1;
#endif
}

KRT_RUNTIME_EXPORT int KRT_API _pow_int(int base, int exp) {
    if (exp < 0) return 0;
    if (exp == 0) return 1;
    int result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrdup(const char* src) {
    if (!src) return NULL;
    
    size_t len = KrtStrlen(src);
    char* dest = (char*)KrtMalloc(len + 1);
    if (!dest) return NULL;
    
    KrtStrcpy(dest, src);
    return dest;
}

KRT_RUNTIME_EXPORT double KRT_API KrtSin(double x) {
    return sin(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtCos(double x) {
    return cos(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtTan(double x) {
    return tan(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtSqrt(double x) {
    return sqrt(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtPow(double base, double exp) {
    return pow(base, exp);
}

KRT_RUNTIME_EXPORT void KRT_API KrtSleep(int seconds) {
    KrtSleepMs(seconds * 1000);
}

KRT_RUNTIME_EXPORT int KRT_API KrtAbs(int x) {
    return (x < 0) ? -x : x;
}

KRT_RUNTIME_EXPORT double KRT_API KrtFabs(double x) {
    return (x < 0) ? -x : x;
}

KRT_RUNTIME_EXPORT double KRT_API KrtAtof(const char* str) {
    if (!str) return 0.0;
    
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0 + (*str - '0');
        str++;
    }
    
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            fraction = fraction * 10.0 + (*str - '0');
            divisor *= 10.0;
            str++;
        }
        result += fraction / divisor;
    }
    
    return sign * result;
}

KRT_RUNTIME_EXPORT char* KRT_API _KrtIntToString_value(int64_t num) {
    static char buffer[64];
    int i = 62;
    int negative = 0;
    
    if (num == INT64_MIN) {
        return KrtStrdup("-9223372036854775808");
    }
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[63] = '\0';
    
    do {
        buffer[i--] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    return KrtStrdup(&buffer[i + 1]);
}

KRT_RUNTIME_EXPORT char* KRT_API KrtIntToString(int64_t num) {
    return _KrtIntToString_value(num);
}

KRT_RUNTIME_EXPORT char* KRT_API _KrtDoubleToStringValue(double num) {
    static char buffer[64];
    
    snprintf(buffer, sizeof(buffer), "%f", num);
    
    char* dot = strchr(buffer, '.');
    if (dot) {
        char* p = buffer + strlen(buffer) - 1;
        while (p > dot && *p == '0') {
            *p-- = '\0';
        }
        if (*p == '.') {
            *p = '\0';
        }
    }
    return KrtStrdup(buffer);
}

KRT_RUNTIME_EXPORT char* KRT_API _KrtStrcat(const char* str1, const char* str2) {
    if (!str1 && !str2) return NULL;
    if (!str1) return KrtStrdup(str2);
    if (!str2) return KrtStrdup(str1);
    
    size_t len1 = KrtStrlen(str1);
    size_t len2 = KrtStrlen(str2);
    char* result = (char*)KrtMalloc(len1 + len2 + 1);
    
    if (!result) return NULL;
    
    KrtStrcpy(result, str1);
    KrtStrcat(result, str2);
    
    return result;
}

#ifdef _WIN32
static ULONGLONG g_timer_start_ticks_dbl = 0;
static ULONGLONG g_timer_start_ticks_int = 0;

KRT_RUNTIME_EXPORT double KRT_API timer_start(void) {
    g_timer_start_ticks_dbl = GetTickCount64();
    return (double)g_timer_start_ticks_dbl / 1000.0;
}

KRT_RUNTIME_EXPORT double KRT_API timer_elapsed(void) {
    if (g_timer_start_ticks_dbl == 0) {
        return -1.0;
    }
    ULONGLONG current = GetTickCount64();
    ULONGLONG elapsed = (current >= g_timer_start_ticks_dbl) ? (current - g_timer_start_ticks_dbl) : (g_timer_start_ticks_dbl - current);
    return (double)elapsed / 1000.0;
}

KRT_RUNTIME_EXPORT double KRT_API timer_current(void) {
    return (double)GetTickCount64() / 1000.0;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_start_int(void) {
    g_timer_start_ticks_int = GetTickCount64();
    return (long long)g_timer_start_ticks_int;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_elapsed_int(void) {
    if (g_timer_start_ticks_int == 0) {
        return -1;
    }
    ULONGLONG current = GetTickCount64();
    ULONGLONG elapsed = (current >= g_timer_start_ticks_int) ? (current - g_timer_start_ticks_int) : (g_timer_start_ticks_int - current);
    return (long long)elapsed;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_current_int(void) {
    return (long long)GetTickCount64();
}
#else
#include <sys/time.h>

static unsigned long long g_timer_start_ticks_dbl = 0;
static unsigned long long g_timer_start_ticks_int = 0;

static unsigned long long get_tick_count_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec) * 1000000ULL + (unsigned long long)(tv.tv_usec);
}

KRT_RUNTIME_EXPORT double KRT_API timer_start(void) {
    g_timer_start_ticks_dbl = get_tick_count_us();
    return (double)g_timer_start_ticks_dbl / 1000000.0;
}

KRT_RUNTIME_EXPORT double KRT_API timer_elapsed(void) {
    if (g_timer_start_ticks_dbl == 0) {
        return -1.0;
    }
    unsigned long long current = get_tick_count_us();
    unsigned long long elapsed = (current >= g_timer_start_ticks_dbl) ? (current - g_timer_start_ticks_dbl) : (g_timer_start_ticks_dbl - current);
    return (double)elapsed / 1000000.0;
}

KRT_RUNTIME_EXPORT double KRT_API timer_current(void) {
    return (double)get_tick_count_us() / 1000000.0;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_start_int(void) {
    g_timer_start_ticks_int = get_tick_count_us();
    return (long long)g_timer_start_ticks_int;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_elapsed_int(void) {
    if (g_timer_start_ticks_int == 0) {
        return -1;
    }
    unsigned long long current = get_tick_count_us();
    unsigned long long elapsed = (current >= g_timer_start_ticks_int) ? (current - g_timer_start_ticks_int) : (g_timer_start_ticks_int - current);
    return (long long)elapsed;
}

KRT_RUNTIME_EXPORT long long KRT_API timer_current_int(void) {
    return (long long)get_tick_count_us();
}
#endif

KRT_RUNTIME_EXPORT void KRT_API KrtPrint(const char* str) {
    _print_string(str);
}

KRT_RUNTIME_EXPORT void KRT_API KrtPrintln(const char* str) {
    _print_string(str);
    _print_newline();
}

KRT_RUNTIME_EXPORT void KRT_API Console__WriteLine(const char* str) {
    _print_string(str);
    _print_newline();
}

KRT_RUNTIME_EXPORT void KRT_API Console__Write(const char* str) {
    _print_string(str);
}

KRT_RUNTIME_EXPORT void KRT_API Console__WriteLineInt(int num) {
    _print_number(num);
    _print_newline();
}

KRT_RUNTIME_EXPORT void KRT_API Console__WriteInt(int num) {
    _print_number(num);
}

KRT_RUNTIME_EXPORT KrtFile KRT_API KrtFopen(const char* filename, const char* mode) {
    if (!filename || !mode) return NULL;

#ifdef _WIN32
    FILE* file = NULL;
    errno_t err = fopen_s(&file, filename, mode);
    return (err == 0) ? (KrtFile)file : NULL;
#else
    return (KrtFile)fopen(filename, mode);
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtFclose(KrtFile file) {
    if (!file) return EOF;

    return fclose((FILE*)file);
}

KRT_RUNTIME_EXPORT size_t KRT_API KrtFread(void* buffer, size_t size, size_t count, KrtFile file) {
    if (!buffer || !file || size == 0 || count == 0) return 0;

    return fread(buffer, size, count, (FILE*)file);
}

KRT_RUNTIME_EXPORT size_t KRT_API KrtFwrite(const void* buffer, size_t size, size_t count, KrtFile file) {
    if (!buffer || !file || size == 0 || count == 0) return 0;

    return fwrite(buffer, size, count, (FILE*)file);
}

KRT_RUNTIME_EXPORT int KRT_API KrtFseek(KrtFile file, long offset, int origin) {
    if (!file) return -1;

    return fseek((FILE*)file, offset, origin);
}

KRT_RUNTIME_EXPORT long KRT_API KrtFtell(KrtFile file) {
    if (!file) return -1;

    return ftell((FILE*)file);
}

KRT_RUNTIME_EXPORT int KRT_API KrtFeof(KrtFile file) {
    if (!file) return 1;
    
    return feof((FILE*)file);
}

KRT_RUNTIME_EXPORT int KRT_API KrtRemove(const char* filename) {
    if (!filename) return -1;
    
#ifdef _WIN32
    return _unlink(filename);
#else
    return remove(filename);
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtRename(const char* oldname, const char* newname) {
    if (!oldname || !newname) return -1;
    
    return rename(oldname, newname);
}

#ifdef _WIN32
static __declspec(thread) char* KrtStrtokLast = NULL;
#else
static __thread char* KrtStrtokLast = NULL;
#endif

KRT_RUNTIME_EXPORT char* KRT_API KrtStrtok(char* str, const char* delim) {
    if (!delim) return NULL;
    
    if (str) {
        KrtStrtokLast = str;
    } else if (!KrtStrtokLast) {
        return NULL;
    }
    
    while (*KrtStrtokLast && KrtStrchr(delim, *KrtStrtokLast)) {
        KrtStrtokLast++;
    }
    
    if (*KrtStrtokLast == '\0') {
        KrtStrtokLast = NULL;
        return NULL;
    }
    
    char* token = KrtStrtokLast;
    
    while (*KrtStrtokLast && !KrtStrchr(delim, *KrtStrtokLast)) {
        KrtStrtokLast++;
    }
    
    if (*KrtStrtokLast) {
        *KrtStrtokLast = '\0';
        KrtStrtokLast++;
    } else {
        KrtStrtokLast = NULL;
    }
    
    return token;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrupr(char* str) {
    if (!str) return NULL;
    
    char* p = str;
    while (*p) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - 'a' + 'A';
        }
        p++;
    }
    
    return str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrlwr(char* str) {
    if (!str) return NULL;
    
    char* p = str;
    while (*p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p - 'A' + 'a';
        }
        p++;
    }
    
    return str;
}

KRT_RUNTIME_EXPORT double KRT_API KrtLog(double x) {
    return log(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtLog10(double x) {
    return log10(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtExp(double x) {
    return exp(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtAsin(double x) {
    return asin(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtAcos(double x) {
    return acos(x);
}

KRT_RUNTIME_EXPORT double KRT_API KrtAtan(double x) {
    return atan(x);
}

KRT_RUNTIME_EXPORT KrtArray* KRT_API array_create(size_t element_size, size_t initial_capacity) {
    if (element_size == 0) return NULL;
    
    KrtArray* array = (KrtArray*)KrtMalloc(sizeof(KrtArray));
    if (!array) return NULL;
    
    if (initial_capacity == 0) {
        initial_capacity = 8;
    }
    
    array->data = KrtMalloc(element_size * initial_capacity);
    if (!array->data) {
        KrtFree(array);
        return NULL;
    }
    
    array->element_size = element_size;
    array->size = 0;
    array->capacity = initial_capacity;
    
    return array;
}

KRT_RUNTIME_EXPORT void KRT_API array_free(KrtArray* array) {
    if (!array) return;
    
    KrtFree(array->data);
    KrtFree(array);
}

KRT_RUNTIME_EXPORT size_t KRT_API array_size(KrtArray* array) {
    if (!array) return 0;
    return array->size;
}

KRT_RUNTIME_EXPORT size_t KRT_API array_capacity(KrtArray* array) {
    if (!array) return 0;
    return array->capacity;
}

KRT_RUNTIME_EXPORT int KRT_API array_resize(KrtArray* array, size_t new_capacity) {
    if (!array || new_capacity == 0) return -1;
    
    if (new_capacity < array->size) {
        array->size = new_capacity;
    }
    
    void* new_data = KrtRealloc(array->data, array->element_size * new_capacity);
    if (!new_data) return -1;
    
    array->data = new_data;
    array->capacity = new_capacity;
    
    return 0;
}

KRT_RUNTIME_EXPORT const char* KRT_API KrtStrerror(KrtErrorCode code) {
    switch (code) {
        case KRT_ERR_NONE:
            return "No error";
        case KRT_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case KRT_ERR_INVALID_ARGUMENT:
            return "Invalid argument";
        case KRT_ERR_FILE_NOT_FOUND:
            return "File not found";
        case KRT_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case KRT_ERR_IO_ERROR:
            return "I/O error";
        case KRT_ERR_INDEX_OUT_OF_BOUNDS:
            return "Index out of bounds";
        case KRT_ERR_DIVISION_BY_ZERO:
            return "Division by zero";
        case KRT_ERR_NULL_POINTER:
            return "Null pointer access";
        case KRT_ERR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case KRT_ERR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

struct KrtHashMap {
    KrtHashMapItem** buckets;
    size_t capacity;
    size_t size;
};

static unsigned int string_hash(const char* str) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    
    return hash;
}

static unsigned int int_hash(int value) {
    return (unsigned int)value;
}

static unsigned int double_hash(double value) {
    union {
        double d;
        unsigned int u[2];
    } converter;
    
    converter.d = value;
    return converter.u[0] ^ converter.u[1];
}

static unsigned int get_hash_value(const void* key, KrtHashMapType type) {
    switch (type) {
        case KRT_HASHMAP_TYPE_INT:
            return int_hash(*(int*)key);
        case KRT_HASHMAP_TYPE_DOUBLE:
            return double_hash(*(double*)key);
        case KRT_HASHMAP_TYPE_STRING:
            return string_hash((const char*)key);
        default:
            return 0;
    }
}

static int keys_equal(const void* key1, KrtHashMapType type1, 
                     const void* key2, KrtHashMapType type2) {
    if (type1 != type2) {
        return 0;
    }
    
    switch (type1) {
        case KRT_HASHMAP_TYPE_INT:
            return *(int*)key1 == *(int*)key2;
        case KRT_HASHMAP_TYPE_DOUBLE:
            return *(double*)key1 == *(double*)key2;
        case KRT_HASHMAP_TYPE_STRING:
            return KrtStrcmp((const char*)key1, (const char*)key2) == 0;
        default:
            return 0;
    }
}

static void* clone_key(const void* key, KrtHashMapType type) {
    switch (type) {
        case KRT_HASHMAP_TYPE_INT: {
            int* new_key = (int*)KrtMalloc(sizeof(int));
            if (new_key) *new_key = *(int*)key;
            return new_key;
        }
        case KRT_HASHMAP_TYPE_DOUBLE: {
            double* new_key = (double*)KrtMalloc(sizeof(double));
            if (new_key) *new_key = *(double*)key;
            return new_key;
        }
        case KRT_HASHMAP_TYPE_STRING:
            return KrtStrdup((const char*)key);
        default:
            return NULL;
    }
}

static void* clone_value(const void* value, KrtHashMapType type) {
    switch (type) {
        case KRT_HASHMAP_TYPE_INT: {
            int* new_value = (int*)KrtMalloc(sizeof(int));
            if (new_value) *new_value = *(int*)value;
            return new_value;
        }
        case KRT_HASHMAP_TYPE_DOUBLE: {
            double* new_value = (double*)KrtMalloc(sizeof(double));
            if (new_value) *new_value = *(double*)value;
            return new_value;
        }
        case KRT_HASHMAP_TYPE_STRING:
            return KrtStrdup((const char*)value);
        default:
            return NULL;
    }
}

static void free_key(void* key, KrtHashMapType type) {
    switch (type) {
        case KRT_HASHMAP_TYPE_INT:
        case KRT_HASHMAP_TYPE_DOUBLE:
            KrtFree(key);
            break;
        case KRT_HASHMAP_TYPE_STRING:
            KrtFree(key);
            break;
        default:
            break;
    }
}

static void free_value(void* value, KrtHashMapType type) {
    switch (type) {
        case KRT_HASHMAP_TYPE_INT:
        case KRT_HASHMAP_TYPE_DOUBLE:
            KrtFree(value);
            break;
        case KRT_HASHMAP_TYPE_STRING:
            KrtFree(value);
            break;
        default:
            break;
    }
}

KRT_RUNTIME_EXPORT KrtHashMap* KRT_API hashmap_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 16; 
    }
    
    KrtHashMap* map = (KrtHashMap*)KrtMalloc(sizeof(KrtHashMap));
    if (!map) return NULL;
    
    map->buckets = (KrtHashMapItem**)KrtCalloc(initial_capacity, sizeof(KrtHashMapItem*));
    if (!map->buckets) {
        KrtFree(map);
        return NULL;
    }
    
    map->capacity = initial_capacity;
    map->size = 0;
    
    return map;
}

KRT_RUNTIME_EXPORT void KRT_API hashmap_free(KrtHashMap* map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        KrtHashMapItem* item = map->buckets[i];
        while (item) {
            KrtHashMapItem* next = item->next;
            
            free_key(item->key, item->key_type);
            free_value(item->value, item->value_type);
            
            KrtFree(item);
            
            item = next;
        }
    }
    
    KrtFree(map->buckets);
    
    KrtFree(map);
}

KRT_RUNTIME_EXPORT int KRT_API hashmap_put(KrtHashMap* map, const void* key, KrtHashMapType key_type, 
                                         const void* value, KrtHashMapType value_type) {
    if (!map || !key || !value) return -1;
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    KrtHashMapItem* item = map->buckets[index];
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            free_value(item->value, item->value_type);
            
            item->value = clone_value(value, value_type);
            if (!item->value) return -1;
            
            item->value_type = value_type;
            return 0; 
        }
        item = item->next;
    }
    
    KrtHashMapItem* new_item = (KrtHashMapItem*)KrtMalloc(sizeof(KrtHashMapItem));
    if (!new_item) return -1;
    
    new_item->key = clone_key(key, key_type);
    if (!new_item->key) {
        KrtFree(new_item);
        return -1;
    }
    
    new_item->value = clone_value(value, value_type);
    if (!new_item->value) {
        free_key(new_item->key, key_type);
        KrtFree(new_item);
        return -1;
    }
    
    new_item->key_type = key_type;
    new_item->value_type = value_type;
    new_item->next = map->buckets[index];
    
    map->buckets[index] = new_item;
    map->size++;
    
    return 0; 
}

KRT_RUNTIME_EXPORT int KRT_API hashmap_get(KrtHashMap* map, const void* key, KrtHashMapType key_type, 
                                        void** out_value, KrtHashMapType* out_value_type) {
    if (!map || !key || !out_value || !out_value_type) return -1;
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    KrtHashMapItem* item = map->buckets[index];
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            *out_value = clone_value(item->value, item->value_type);
            if (!*out_value) return -1;
            
            *out_value_type = item->value_type;
            return 0; 
        }
        item = item->next;
    }
    
    *out_value = NULL;
    *out_value_type = KRT_HASHMAP_TYPE_INT; 
    return -1;
}

KRT_RUNTIME_EXPORT int KRT_API hashmap_remove(KrtHashMap* map, const void* key, KrtHashMapType key_type) {
    if (!map || !key) return -1;
    
    unsigned int hash = get_hash_value(key, key_type);
    size_t index = hash % map->capacity;
    
    KrtHashMapItem* item = map->buckets[index];
    KrtHashMapItem* prev = NULL;
    
    while (item) {
        if (keys_equal(item->key, item->key_type, key, key_type)) {
            
            if (prev) {
                prev->next = item->next;
            } else {
                map->buckets[index] = item->next;
            }
            
            free_key(item->key, item->key_type);
            free_value(item->value, item->value_type);
            
            KrtFree(item);
            
            map->size--;
            return 0; 
        }
        
        prev = item;
        item = item->next;
    }
    
    return -1;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtDate() {
    
    time_t current_time;
    time(&current_time);
    
#ifdef _WIN32
    struct tm local_time;
    if (localtime_s(&local_time, &current_time) != 0) return NULL;
#else
    struct tm local_time;
    if (localtime_r(&current_time, &local_time) == NULL) return NULL;
#endif
    
    char* date_str = (char*)KrtMalloc(11); 
    if (!date_str) return NULL;
    
    KRT_SPRINTF_S(date_str, 11, "%04d-%02d-%02d", 
            local_time.tm_year + 1900, 
            local_time.tm_mon + 1, 
            local_time.tm_mday);
    
    return date_str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtTimeFormat(const char* format) {
    if (!format) return NULL;
    
    time_t current_time;
    time(&current_time);
    
#ifdef _WIN32
    struct tm local_time;
    if (localtime_s(&local_time, &current_time) != 0) return NULL;
#else
    struct tm local_time;
    if (localtime_r(&current_time, &local_time) == NULL) return NULL;
#endif
    
    size_t format_len = KrtStrlen(format);
    size_t result_len = format_len * 3; 
    char* result = (char*)KrtMalloc(result_len + 1); 
    if (!result) return NULL;
    
    size_t result_index = 0;
    for (size_t i = 0; i < format_len && result_index < result_len; i++) {
        if (format[i] == '%' && i + 1 < format_len) {
            char format_char = format[i + 1];
            switch (format_char) {
                case 'Y': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%04d", local_time.tm_year + 1900);
                    i++;
                    break;
                case 'y': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", (local_time.tm_year + 1900) % 100);
                    i++;
                    break;
                case 'm': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time.tm_mon + 1);
                    i++;
                    break;
                case 'd': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time.tm_mday);
                    i++;
                    break;
                case 'H': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time.tm_hour);
                    i++;
                    break;
                case 'M': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time.tm_min);
                    i++;
                    break;
                case 'S': 
                    result_index += KRT_SPRINTF_S(result + result_index, result_len - result_index + 1, "%02d", local_time.tm_sec);
                    i++;
                    break;
                default:
                    
                    result[result_index++] = format[i];
                    break;
            }
        } else {
            
            result[result_index++] = format[i];
        }
    }
    
    result[result_index] = '\0';
    
    char* final_result = KrtRealloc(result, result_index + 1);
    if (final_result) {
        return final_result;
    } else {
        
        return result;
    }
}

KRT_RUNTIME_EXPORT int KRT_API array_append(KrtArray* array, const void* element) {
    if (!array || !element) return -1;
    
    if (array->size >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        if (array_resize(array, new_capacity) != 0) {
            return -1;
        }
    }
    
    char* dest = (char*)array->data + array->size * array->element_size;
    for (size_t i = 0; i < array->element_size; i++) {
        dest[i] = ((char*)element)[i];
    }
    
    array->size++;
    return 0;
}

KRT_RUNTIME_EXPORT void* KRT_API array_get(KrtArray* array, size_t index) {
    if (!array || index >= array->size) return NULL;
    
    return (char*)array->data + index * array->element_size;
}

KRT_RUNTIME_EXPORT int KRT_API array_set(KrtArray* array, size_t index, const void* element) {
    if (!array || !element || index >= array->size) return -1;
    
    char* dest = (char*)array->data + index * array->element_size;
    for (size_t i = 0; i < array->element_size; i++) {
        dest[i] = ((char*)element)[i];
    }
    
    return 0;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrrev(char* str) {
    if (!str) return NULL;
    
    size_t len = KrtStrlen(str);
    if (len <= 1) return str;
    
    char* start = str;
    char* end = str + len - 1;
    
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
    
    return str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrtrim(char* str) {
    if (!str) return NULL;
    
    char* end = str + KrtStrlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    if (start != str) {
        char* p = str;
        while (*start) {
            *p++ = *start++;
        }
        *p = '\0';
    }
    
    return str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrltrim(char* str) {
    if (!str) return NULL;
    
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    if (start != str) {
        char* p = str;
        while (*start) {
            *p++ = *start++;
        }
        *p = '\0';
    }
    
    return str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrrtrim(char* str) {
    if (!str) return NULL;
    
    char* end = str + KrtStrlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    return str;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStrreplace(const char* src, const char* old_sub, const char* new_sub) {
    if (!src || !old_sub) return NULL;
    
    if (*old_sub == '\0') {
        return KrtStrdup(src);
    }
    
    const char* replacement = new_sub ? new_sub : "";
    
    size_t src_len = KrtStrlen(src);
    size_t old_len = KrtStrlen(old_sub);
    size_t new_len = KrtStrlen(replacement);
    
    size_t max_len = src_len;
    const char* p = src;
    while ((p = KrtStrstr(p, old_sub)) != NULL) {
        max_len = max_len - old_len + new_len;
        p += old_len;
    }
    
    char* result = (char*)KrtMalloc(max_len + 1);
    if (!result) return NULL;
    
    char* dest = result;
    const char* src_pos = src;
    
    while (*src_pos) {
        const char* match = KrtStrstr(src_pos, old_sub);
        if (match) {
            
            size_t copy_len = match - src_pos;
            for (size_t i = 0; i < copy_len; i++) {
                *dest++ = *src_pos++;
            }
            
            src_pos += old_len;
            
            for (size_t i = 0; i < new_len; i++) {
                *dest++ = replacement[i];
            }
        } else {
            
            while (*src_pos) {
                *dest++ = *src_pos++;
            }
            break;
        }
    }
    
    *dest = '\0';
    return result;
}

KRT_RUNTIME_EXPORT double KRT_API KrtSinh(double x) {
    
    double exp_x = KrtExp(x);
    double exp_neg_x = KrtExp(-x);
    return (exp_x - exp_neg_x) / 2.0;
}

KRT_RUNTIME_EXPORT double KRT_API KrtCosh(double x) {
    
    double exp_x = KrtExp(x);
    double exp_neg_x = KrtExp(-x);
    return (exp_x + exp_neg_x) / 2.0;
}

KRT_RUNTIME_EXPORT double KRT_API KrtTanh(double x) {
    
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    
    double sinh_val = KrtSinh(x);
    double cosh_val = KrtCosh(x);
    
    if (cosh_val == 0.0) {
        return (sinh_val > 0) ? 1.0 : -1.0;
    }
    
    return sinh_val / cosh_val;
}

#ifdef _WIN32
#else
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

static int network_initialized = 0;
static void init_network() {
    if (!network_initialized) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return;
        }
#endif
        network_initialized = 1;
    }
}

KRT_RUNTIME_EXPORT KrtSocketHandle KRT_API KrtSocketCreate(int domain, int type, int protocol) {
    init_network();
    
    SOCKET sock = socket(domain, type, protocol);
    if (sock == INVALID_SOCKET) {
        return NULL;
    }
    
    SOCKET* socket_ptr = (SOCKET*)KrtMalloc(sizeof(SOCKET));
    if (!socket_ptr) {
        closesocket(sock);
        return NULL;
    }
    
    *socket_ptr = sock;
    return (KrtSocketHandle)socket_ptr;
}

KRT_RUNTIME_EXPORT int KRT_API KrtConnect(KrtSocketHandle socket, const char* address, int port) {
    if (!socket || !address) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int ret = getaddrinfo(address, port_str, &hints, &result);
    if (ret != 0) return -1;
    
    int connected = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
            connected = 0;
            break;
        }
    }
    
    freeaddrinfo(result);
    return connected;
}

KRT_RUNTIME_EXPORT int KRT_API KrtBind(KrtSocketHandle socket, const char* address, int port) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (!address || *address == '\0') {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
            return -1;
        }
    }
    
    return bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0 ? 0 : -1;
}

KRT_RUNTIME_EXPORT int KRT_API KrtListen(KrtSocketHandle socket, int backlog) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return listen(sock, backlog) == 0 ? 0 : -1;
}

KRT_RUNTIME_EXPORT KrtSocketHandle KRT_API KrtAccept(KrtSocketHandle socket) {
    if (!socket) return NULL;
    
    SOCKET sock = *(SOCKET*)socket;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    SOCKET client_sock = accept(sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        return NULL;
    }
    
    SOCKET* socket_ptr = (SOCKET*)KrtMalloc(sizeof(SOCKET));
    if (!socket_ptr) {
        closesocket(client_sock);
        return NULL;
    }
    
    *socket_ptr = client_sock;
    return (KrtSocketHandle)socket_ptr;
}

KRT_RUNTIME_EXPORT int KRT_API KrtSend(KrtSocketHandle socket, const char* data, size_t length) {
    if (!socket || !data) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return send(sock, data, length, 0);
}

KRT_RUNTIME_EXPORT int KRT_API KrtRecv(KrtSocketHandle socket, char* buffer, size_t length) {
    if (!socket || !buffer) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    return recv(sock, buffer, length, 0);
}

KRT_RUNTIME_EXPORT int KRT_API KrtCloseSocket(KrtSocketHandle socket) {
    if (!socket) return -1;
    
    SOCKET sock = *(SOCKET*)socket;
    int result = closesocket(sock);
    
    KrtFree(socket);
    
    return result == 0 ? 0 : -1;
}

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
DWORD WINAPI thread_wrapper(LPVOID arg) {
    void** params = (void**)arg;
    void (*func)(void*) = (void (*)(void*))params[0];
    void* func_arg = params[1];
    KrtFree(params);
    
    func(func_arg);
    return 0;
}
#else
void* thread_wrapper(void* arg) {
    void** params = (void**)arg;
    void (*func)(void*) = (void (*)(void*))params[0];
    void* func_arg = params[1];
    KrtFree(params);
    
    func(func_arg);
    return NULL;
}
#endif

KRT_RUNTIME_EXPORT KrtThread KRT_API KrtThreadCreateFunc(void (*func)(void*), void* arg) {
    if (!func) return NULL;
    
    void** params = (void**)KrtMalloc(2 * sizeof(void*));
    if (!params) return NULL;
    
    params[0] = (void*)func;
    params[1] = arg;
    
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, thread_wrapper, params, 0, NULL);
    if (!thread) {
        KrtFree(params);
        return NULL;
    }
    
    HANDLE* thread_ptr = (HANDLE*)KrtMalloc(sizeof(HANDLE));
    if (!thread_ptr) {
        CloseHandle(thread);
        KrtFree(params);
        return NULL;
    }
    
    *thread_ptr = thread;
    return (KrtThread)thread_ptr;
#else
    pthread_t* thread_ptr = (pthread_t*)KrtMalloc(sizeof(pthread_t));
    if (!thread_ptr) {
        KrtFree(params);
        return NULL;
    }
    
    if (pthread_create(thread_ptr, NULL, thread_wrapper, params) != 0) {
        KrtFree(thread_ptr);
        KrtFree(params);
        return NULL;
    }
    
    return (KrtThread)thread_ptr;
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtThreadJoinFunc(KrtThread thread) {
    if (!thread) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)thread;
    DWORD result = WaitForSingleObject(h, INFINITE);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(h);
        KrtFree(thread);
        return 0;
    }
    return -1;
#else
    pthread_t t = *(pthread_t*)thread;
    int result = pthread_join(t, NULL);
    KrtFree(thread);
    return result == 0 ? 0 : -1;
#endif
}

KRT_RUNTIME_EXPORT void KRT_API KrtThreadExitFunc(void) {
#ifdef _WIN32
    ExitThread(0);
#else
    pthread_exit(NULL);
#endif
}

KRT_RUNTIME_EXPORT KrtMutex KRT_API KrtMutexCreateFunc(void) {
#ifdef _WIN32
    HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex) return NULL;
    
    HANDLE* mutex_ptr = (HANDLE*)KrtMalloc(sizeof(HANDLE));
    if (!mutex_ptr) {
        CloseHandle(mutex);
        return NULL;
    }
    
    *mutex_ptr = mutex;
    return (KrtMutex)mutex_ptr;
#else
    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)KrtMalloc(sizeof(pthread_mutex_t));
    if (!mutex_ptr) return NULL;
    
    if (pthread_mutex_init(mutex_ptr, NULL) != 0) {
        KrtFree(mutex_ptr);
        return NULL;
    }
    
    return (KrtMutex)mutex_ptr;
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtMutexLockFunc(KrtMutex mutex) {
    if (!mutex) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    DWORD result = WaitForSingleObject(h, INFINITE);
    return result == WAIT_OBJECT_0 ? 0 : -1;
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return pthread_mutex_lock(m) == 0 ? 0 : -1;
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtMutexUnlockFunc(KrtMutex mutex) {
    if (!mutex) return -1;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    return ReleaseMutex(h) ? 0 : -1;
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return pthread_mutex_unlock(m) == 0 ? 0 : -1;
#endif
}

KRT_RUNTIME_EXPORT void KRT_API KrtMutexFreeFunc(KrtMutex mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    HANDLE h = *(HANDLE*)mutex;
    CloseHandle(h);
#else
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    pthread_mutex_destroy(m);
#endif
    
    KrtFree(mutex);
}

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#else

#ifdef HAS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#endif

typedef struct {
#ifdef _WIN32
    HWND hwnd;
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    void* bitmap_data;
#else
#ifdef HAS_X11
    Display* display;
    Window window;
    GC gc;
    XImage* image;
#endif
    void* bitmap_data;
#endif
    int width;
    int height;
    int is_open;
    char* title;
} KrtWindowImpl;

KRT_RUNTIME_EXPORT KrtWindow KRT_API KrtWindowCreate(int width, int height, const char* title) {
    if (width <= 0 || height <= 0) return NULL;
    
    KrtWindowImpl* window = (KrtWindowImpl*)KrtMalloc(sizeof(KrtWindowImpl));
    if (!window) return NULL;
    
    memset(window, 0, sizeof(KrtWindowImpl));
    window->width = width;
    window->height = height;
    window->is_open = 0;
    
    if (title) {
        window->title = KrtStrdup(title);
    } else {
        window->title = KrtStrdup("Kairote Lang Window");
    }
    
#ifdef _WIN32
    
    static int class_registered = 0;
    if (!class_registered) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "ESWindow";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        
        if (!RegisterClass(&wc)) {
            KrtFree(window->title);
            KrtFree(window);
            return NULL;
        }
        class_registered = 1;
    }
    
    window->hwnd = CreateWindow(
        "ESWindow",
        window->title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!window->hwnd) {
        KrtFree(window->title);
        KrtFree(window);
        return NULL;
    }
    
    window->hdc = CreateCompatibleDC(NULL);
    window->bitmap = CreateCompatibleBitmap(window->hdc, width, height);
    window->old_bitmap = SelectObject(window->hdc, window->bitmap);
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    window->bitmap_data = KrtMalloc(width * height * 4);
    if (!window->bitmap_data) {
        SelectObject(window->hdc, window->old_bitmap);
        DeleteObject(window->bitmap);
        DeleteDC(window->hdc);
        DestroyWindow(window->hwnd);
        KrtFree(window->title);
        KrtFree(window);
        return NULL;
    }
    
    SetDIBits(window->hdc, window->bitmap, 0, height, window->bitmap_data, &bmi, DIB_RGB_COLORS);
#else
#ifdef HAS_X11
    
    window->display = XOpenDisplay(NULL);
    if (!window->display) {
        KrtFree(window->title);
        KrtFree(window);
        return NULL;
    }
    
    int screen = DefaultScreen(window->display);
    window->window = XCreateSimpleWindow(
        window->display, 
        RootWindow(window->display, screen),
        0, 0, width, height, 1,
        BlackPixel(window->display, screen),
        WhitePixel(window->display, screen)
    );
    
    XStoreName(window->display, window->window, window->title);
    window->gc = XCreateGC(window->display, window->window, 0, NULL);
    
    window->bitmap_data = KrtMalloc(width * height * 4);
    if (!window->bitmap_data) {
        XFreeGC(window->display, window->gc);
        XDestroyWindow(window->display, window->window);
        XCloseDisplay(window->display);
        KrtFree(window->title);
        KrtFree(window);
        return NULL;
    }
    
    Visual* visual = DefaultVisual(window->display, screen);
    window->image = XCreateImage(
        window->display, visual, 24, ZPixmap, 0,
        (char*)window->bitmap_data, width, height, 32, 0
    );
#else
    
    KrtFree(window->title);
    KrtFree(window);
    return NULL;
#endif
#endif
    
    return (KrtWindow)window;
}

KRT_RUNTIME_EXPORT void KRT_API KrtWindowShow(KrtWindow window) {
    if (!window) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    win->is_open = 1;
    
#ifdef _WIN32
    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
#else
#ifdef HAS_X11
    XMapWindow(win->display, win->window);
    XFlush(win->display);
#endif
#endif
}

KRT_RUNTIME_EXPORT void KRT_API KrtWindowClose(KrtWindow window) {
    if (!window) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    win->is_open = 0;
    
#ifdef _WIN32
    DestroyWindow(win->hwnd);
#else
#ifdef HAS_X11
    XDestroyWindow(win->display, win->window);
    XCloseDisplay(win->display);
#endif
#endif
}

KRT_RUNTIME_EXPORT void KRT_API KrtWindowClear(KrtWindow window, KrtColor color) {
    if (!window) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    for (int i = 0; i < win->width * win->height; i++) {
        pixels[i] = pixel_color;
    }
}

KRT_RUNTIME_EXPORT void KRT_API KrtWindowUpdate(KrtWindow window) {
    if (!window) return;
    
#ifdef _WIN32
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = win->width;
    bmi.bmiHeader.biHeight = -win->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    SetDIBits(win->hdc, win->bitmap, 0, win->height, win->bitmap_data, &bmi, DIB_RGB_COLORS);
    
    HDC hdc = GetDC(win->hwnd);
    BitBlt(hdc, 0, 0, win->width, win->height, win->hdc, 0, 0, SRCCOPY);
    ReleaseDC(win->hwnd, hdc);
#else
#ifdef HAS_X11
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    
    XPutImage(win->display, win->window, win->gc, win->image, 0, 0, 0, 0, win->width, win->height);
    XFlush(win->display);
#endif
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtWindowIsOpen(KrtWindow window) {
    if (!window) return 0;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    return win->is_open;
}

KRT_RUNTIME_EXPORT void KRT_API KrtDrawRect(KrtWindow window, KrtRect rect, KrtColor color) {
    if (!window) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    int left = rect.x < 0 ? 0 : rect.x;
    int top = rect.y < 0 ? 0 : rect.y;
    int right = (rect.x + rect.width) > win->width ? win->width : rect.x + rect.width;
    int bottom = (rect.y + rect.height) > win->height ? win->height : rect.y + rect.height;
    
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            pixels[y * win->width + x] = pixel_color;
        }
    }
}

KRT_RUNTIME_EXPORT void KRT_API KrtDrawCircle(KrtWindow window, KrtPoint center, int radius, KrtColor color) {
    if (!window || radius <= 0) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    
    while (x <= y) {
        
        for (int i = -x; i <= x; i++) {
            
            if (center.y + y >= 0 && center.y + y < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y + y) * win->width + (center.x + i)] = pixel_color;
            }
            if (center.y - y >= 0 && center.y - y < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y - y) * win->width + (center.x + i)] = pixel_color;
            }
        }
        
        for (int i = -y; i <= y; i++) {
            
            if (center.y + x >= 0 && center.y + x < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y + x) * win->width + (center.x + i)] = pixel_color;
            }
            if (center.y - x >= 0 && center.y - x < win->height && 
                center.x + i >= 0 && center.x + i < win->width) {
                pixels[(center.y - x) * win->width + (center.x + i)] = pixel_color;
            }
        }
        
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

KRT_RUNTIME_EXPORT void KRT_API KrtDrawText(KrtWindow window, const char* text, KrtPoint position, KrtColor color) {
    if (!window || !text) return;
    
    KrtWindowImpl* win = (KrtWindowImpl*)window;
    unsigned int pixel_color = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    unsigned int* pixels = (unsigned int*)win->bitmap_data;
    
    int char_width = 8;
    int char_height = 16;
    
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) continue; 
        
        int char_x = position.x + i * char_width;
        int char_y = position.y;
        
        for (int row = 0; row < char_height; row++) {
            unsigned char font_row = font_data[c - 32][row];
            for (int col = 0; col < char_width; col++) {
                if (font_row & (0x80 >> col)) {
                    int pixel_x = char_x + col;
                    int pixel_y = char_y + row;
                    
                    if (pixel_x >= 0 && pixel_x < win->width && 
                        pixel_y >= 0 && pixel_y < win->height) {
                        pixels[pixel_y * win->width + pixel_x] = pixel_color;
                    }
                }
            }
        }
    }
}

uint64_t get_total_memory(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.totalram * info.mem_unit;
    }
    return 0;
#endif
}

uint64_t get_free_memory(void) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.freeram * info.mem_unit;
    }
    return 0;
#endif
}

KRT_RUNTIME_EXPORT int KRT_API KrtIsInstance(void* obj, const char* typeName) {
    if (!obj || !typeName) return 0;
    
    void** vtable = (void**)obj;
    if (!vtable || !*vtable) return 0;
    
    char** typeNamePtr = (char**)(*vtable);
    if (!typeNamePtr || !*typeNamePtr) return 0;
    
    return KrtStrcmp(*typeNamePtr, typeName) == 0;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtAsInstance(void* obj, const char* typeName) {
    if (!obj || !typeName) return NULL;
    
    if (KrtIsInstance(obj, typeName)) {
        return obj;
    }
    
    return NULL;
}

#define MONITOR_LOCK_TABLE_SIZE 256

typedef struct MonitorLockEntry {
    void* obj;
    KrtMutex mutex;
    int ref_count;
    struct MonitorLockEntry* next;
} MonitorLockEntry;

static KrtMutex g_monitor_table_mutex = NULL;
static MonitorLockEntry* g_monitor_lock_table[MONITOR_LOCK_TABLE_SIZE] = {0};

static unsigned int monitor_hash(void* obj) {
    uintptr_t addr = (uintptr_t)obj;
    return (unsigned int)(addr % MONITOR_LOCK_TABLE_SIZE);
}

static KrtMutex monitor_get_lock(void* obj) {
    unsigned int hash = monitor_hash(obj);
    
    MonitorLockEntry* entry = g_monitor_lock_table[hash];
    while (entry) {
        if (entry->obj == obj) {
            entry->ref_count++;
            return entry->mutex;
        }
        entry = entry->next;
    }
    
    KrtMutex new_mutex = KrtMutexCreateFunc();
    if (!new_mutex) return NULL;
    
    MonitorLockEntry* new_entry = (MonitorLockEntry*)KrtMalloc(sizeof(MonitorLockEntry));
    if (!new_entry) {
        KrtMutexFreeFunc(new_mutex);
        return NULL;
    }
    new_entry->obj = obj;
    new_entry->mutex = new_mutex;
    new_entry->ref_count = 1;
    new_entry->next = g_monitor_lock_table[hash];
    g_monitor_lock_table[hash] = new_entry;
    
    return new_mutex;
}

static void monitor_release_lock(void* obj) {
    unsigned int hash = monitor_hash(obj);
    
    MonitorLockEntry* entry = g_monitor_lock_table[hash];
    MonitorLockEntry* prev = NULL;
    while (entry) {
        if (entry->obj == obj) {
            entry->ref_count--;
            if (entry->ref_count <= 0) {
                KrtMutexFreeFunc(entry->mutex);
                if (prev) {
                    prev->next = entry->next;
                } else {
                    g_monitor_lock_table[hash] = entry->next;
                }
                KrtFree(entry);
            }
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

KRT_RUNTIME_EXPORT void KRT_API Monitor_Enter(void* obj) {
    if (!obj) return;
    
    if (!g_monitor_table_mutex) {
        g_monitor_table_mutex = KrtMutexCreateFunc();
    }
    
    KrtMutexLockFunc(g_monitor_table_mutex);
    KrtMutex lock = monitor_get_lock(obj);
    KrtMutexUnlockFunc(g_monitor_table_mutex);
    
    if (lock) {
        KrtMutexLockFunc(lock);
    }
}

KRT_RUNTIME_EXPORT void KRT_API Monitor_Exit(void* obj) {
    if (!obj) return;
    
    if (!g_monitor_table_mutex) return;
    
    KrtMutexLockFunc(g_monitor_table_mutex);
    KrtMutex lock = NULL;
    
    unsigned int hash = monitor_hash(obj);
    MonitorLockEntry* entry = g_monitor_lock_table[hash];
    while (entry) {
        if (entry->obj == obj) {
            lock = entry->mutex;
            break;
        }
        entry = entry->next;
    }
    
    if (lock) {
        KrtMutexUnlockFunc(lock);
    }
    
    monitor_release_lock(obj);
    KrtMutexUnlockFunc(g_monitor_table_mutex);
}

KRT_RUNTIME_EXPORT void KRT_API Dispose(void* obj) {
    if (!obj) return;
    
    void** vtable = (void**)obj;
    if (vtable && *vtable) {
        void (*disposeFunc)(void*) = (void (*)(void*))((void**)*vtable)[1];
        if (disposeFunc) {
            disposeFunc(obj);
        }
    }
}

KRT_RUNTIME_EXPORT void* KRT_API KrtNullCoalesce(void* left, void* right) {
    if (left != NULL) {
        return left;
    }
    return right;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtNullConditional(void* obj, const char* memberName) {
    if (!obj || !memberName) return NULL;
    
    void** vtable = (void**)obj;
    if (!vtable || !*vtable) return NULL;
    
    return obj;
}

#ifdef _WIN32
static __declspec(thread) void* g_current_exception = NULL;
#else
static __thread void* g_current_exception = NULL;
#endif

KRT_RUNTIME_EXPORT void KRT_API KrtThrowException(void* exception) {
    g_current_exception = exception;
    // In a real implementation, this would unwind the stack
    // For now, we just store the exception
}

KRT_RUNTIME_EXPORT void KRT_API KrtRethrowException(void) {
    // Rethrow the current exception
    // The exception is already in g_current_exception
    // In a real implementation, this would continue unwinding
}

KRT_RUNTIME_EXPORT void* KRT_API KrtGetCurrentException(void) {
    return g_current_exception;
}

KRT_RUNTIME_EXPORT void KRT_API KrtSetCurrentException(void* exception) {
    g_current_exception = exception;
}

KRT_RUNTIME_EXPORT int KRT_API KrtCastToInt32(double value) {
    return (int)value;
}

KRT_RUNTIME_EXPORT long long KRT_API KrtCastToInt64(double value) {
    return (long long)value;
}

KRT_RUNTIME_EXPORT float KRT_API KrtCastToFloat32(double value) {
    return (float)value;
}

KRT_RUNTIME_EXPORT double KRT_API KrtCastToFloat64(double value) {
    return value;
}

KRT_RUNTIME_EXPORT int KRT_API KrtCastToBool(double value) {
    return value != 0.0 ? 1 : 0;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtCastToString(double value) {
    char* result = (char*)KrtMalloc(32);
    if (result) {
        snprintf(result, 32, "%g", value);
    }
    return result;
}

KRT_RUNTIME_EXPORT void KRT_API KrtOrderBy(void* array, int count, size_t element_size, KrtCompareFunc compare) {
    if (!array || count <= 1 || !compare) return;
    qsort(array, (size_t)count, element_size, compare);
}

KRT_RUNTIME_EXPORT int KRT_API KrtCompareInt32Asc(const void* a, const void* b) {
    int32_t av = *(const int32_t*)a;
    int32_t bv = *(const int32_t*)b;
    return (av > bv) - (av < bv);
}

KRT_RUNTIME_EXPORT int KRT_API KrtCompareInt32Desc(const void* a, const void* b) {
    int32_t av = *(const int32_t*)a;
    int32_t bv = *(const int32_t*)b;
    return (bv > av) - (bv < av);
}

KRT_RUNTIME_EXPORT int KRT_API KrtCompareFloat64Asc(const void* a, const void* b) {
    double av = *(const double*)a;
    double bv = *(const double*)b;
    return (av > bv) - (av < bv);
}

KRT_RUNTIME_EXPORT int KRT_API KrtCompareFloat64Desc(const void* a, const void* b) {
    double av = *(const double*)a;
    double bv = *(const double*)b;
    return (bv > av) - (bv < av);
}

KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfInt32(void) {
    return sizeof(int32_t);
}

KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfInt64(void) {
    return sizeof(int64_t);
}

KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfFloat32(void) {
    return sizeof(float);
}

KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfFloat64(void) {
    return sizeof(double);
}

KRT_RUNTIME_EXPORT int KRT_API KrtSizeOfPointer(void) {
    return sizeof(void*);
}

KRT_RUNTIME_EXPORT char* KRT_API KrtStringConcat(const char* s1, const char* s2) {
    if (!s1 && !s2) return NULL;
    if (!s1) return KRT_STRDUP(s2);
    if (!s2) return KRT_STRDUP(s1);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = (char*)KRT_MALLOC(len1 + len2 + 1);
    if (!result) return NULL;
    
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtInt32ToString(int32_t value) {
    char* result = (char*)KRT_MALLOC(16);
    if (!result) return NULL;
    KRT_SPRINTF_S(result, 16, "%d", value);
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtInt64ToString(int64_t value) {
    char* result = (char*)KRT_MALLOC(32);
    if (!result) return NULL;
    KRT_SPRINTF_S(result, 32, "%lld", (long long)value);
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtFloat32ToString(float value) {
    char* result = (char*)KRT_MALLOC(32);
    if (!result) return NULL;
    KRT_SPRINTF_S(result, 32, "%g", value);
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtFloat64ToString(double value) {
    char* result = (char*)KRT_MALLOC(32);
    if (!result) return NULL;
    KRT_SPRINTF_S(result, 32, "%g", value);
    return result;
}

KRT_RUNTIME_EXPORT char* KRT_API KrtBoolToString(int value) {
    return KRT_STRDUP(value ? "True" : "False");
}

KRT_RUNTIME_EXPORT char* KRT_API KrtPointerToString(void* value) {
    char* result = (char*)KRT_MALLOC(32);
    if (!result) return NULL;
    KRT_SPRINTF_S(result, 32, "%p", value);
    return result;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtCreateTuple(int element_count) {
    if (element_count <= 0) return NULL;
    
    void** tuple = (void**)KRT_MALLOC(element_count * sizeof(void*));
    if (!tuple) return NULL;
    
    for (int i = 0; i < element_count; i++) {
        tuple[i] = NULL;
    }
    
    return tuple;
}

KRT_RUNTIME_EXPORT void KRT_API KrtTupleSetElement(void* tuple, int index, void* value) {
    if (!tuple || index < 0) return;
    
    void** elements = (void**)tuple;
    elements[index] = value;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtTupleGetElement(void* tuple, int index) {
    if (!tuple || index < 0) return NULL;
    
    void** elements = (void**)tuple;
    return elements[index];
}

KRT_RUNTIME_EXPORT void* KRT_API KrtGetVariableAddress(const char* var_name) {
    // This is a placeholder in real implementation would need
    // access to variable storage. For now returns NULL.
    (void)var_name;
    return NULL;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtLoadPtr(void* base, int offset) {
    if (!base) return NULL;
    char* ptr = (char*)base;
    return *(void**)(ptr + offset);
}

KRT_RUNTIME_EXPORT void KRT_API KrtStorePtr(void* base, int offset, void* value) {
    if (!base) return;
    char* ptr = (char*)base;
    *(void**)(ptr + offset) = value;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtStackAlloc(int size) {
    if (size <= 0) return NULL;
    // In a real implementation, this would allocate from the stack
    // For now, we use malloc
    return KrtMalloc((size_t)size);
}

static void** g_pinned_objects = NULL;
static int g_pinned_count = 0;
static int g_pinned_capacity = 0;

KRT_RUNTIME_EXPORT void KRT_API KrtPinObject(void* obj) {
    if (!obj) return;
    
    if (!g_pinned_objects) {
        g_pinned_capacity = 16;
        g_pinned_objects = (void**)KrtMalloc(sizeof(void*) * g_pinned_capacity);
        if (!g_pinned_objects) return;
    }
    
    if (g_pinned_count >= g_pinned_capacity) {
        int new_capacity = g_pinned_capacity * 2;
        void** new_array = (void**)KrtRealloc(g_pinned_objects, sizeof(void*) * new_capacity);
        if (!new_array) return;
        g_pinned_objects = new_array;
        g_pinned_capacity = new_capacity;
    }
    
    g_pinned_objects[g_pinned_count++] = obj;
}

KRT_RUNTIME_EXPORT void KRT_API KrtUnpinObject(void* obj) {
    if (!obj || !g_pinned_objects) return;
    
    for (int i = 0; i < g_pinned_count; i++) {
        if (g_pinned_objects[i] == obj) {
            for (int j = i; j < g_pinned_count - 1; j++) {
                g_pinned_objects[j] = g_pinned_objects[j + 1];
            }
            g_pinned_count--;
            return;
        }
    }
}

typedef struct KrtTask {
    int state;
    void* result;
    void (*continuation)(void*);
    void* continuation_arg;
    int is_completed;
} KrtTask;

KRT_RUNTIME_EXPORT void* KRT_API KrtCreateTask(void) {
    KrtTask* task = (KrtTask*)KrtMalloc(sizeof(KrtTask));
    if (!task) return NULL;
    task->state = 0;
    task->result = NULL;
    task->continuation = NULL;
    task->continuation_arg = NULL;
    task->is_completed = 0;
    return task;
}

KRT_RUNTIME_EXPORT void KRT_API KrtCompleteTask(void* task_handle, void* result) {
    if (!task_handle) return;
    KrtTask* task = (KrtTask*)task_handle;
    task->result = result;
    task->is_completed = 1;
    if (task->continuation) {
        task->continuation(task->continuation_arg);
    }
}

KRT_RUNTIME_EXPORT void* KRT_API KrtAwaitTask(void* task_handle) {
    if (!task_handle) return NULL;
    KrtTask* task = (KrtTask*)task_handle;
    
    // In a real implementation, this would yield control
    // For now, we just wait (busy wait - not ideal but works for basic cases)
    while (!task->is_completed) {
        KrtSleepMs(1);
    }
    
    return task->result;
}

KRT_RUNTIME_EXPORT int KRT_API KrtTaskIsCompleted(void* task_handle) {
    if (!task_handle) return 0;
    KrtTask* task = (KrtTask*)task_handle;
    return task->is_completed;
}


KRT_RUNTIME_EXPORT void* KRT_API KrtLinqWhere(void* source, void* predicate) {
    // Placeholder - would filter elements based on predicate
    (void)predicate;
    return source;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtLinqOrderBy(void* source, int ascending, void* key_selector) {
    // Placeholder - would sort elements
    (void)ascending;
    (void)key_selector;
    return source;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtLinqGroupBy(void* source, void* key, void* element, const char* into_var) {
    // Placeholder - would group elements
    (void)key;
    (void)element;
    (void)into_var;
    return source;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtLinqSelect(void* source, void* selector) {
    // Placeholder - would project elements
    (void)selector;
    return source;
}


typedef struct KrtDelegate {
    void* target;
    void* method_ptr;
    char* method_name;
} KrtDelegate;

KRT_RUNTIME_EXPORT void* KRT_API KrtCreateDelegate(void* target, void* method_ptr, const char* method_name) {
    KrtDelegate* delegate = (KrtDelegate*)KrtMalloc(sizeof(KrtDelegate));
    if (!delegate) return NULL;
    
    delegate->target = target;
    delegate->method_ptr = method_ptr;
    delegate->method_name = method_name ? KrtStrdup(method_name) : NULL;
    
    return delegate;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtInvokeDelegate(void* delegate_handle, void** args, int arg_count) {
    if (!delegate_handle) return NULL;
    KrtDelegate* delegate = (KrtDelegate*)delegate_handle;
    
    if (!delegate->method_ptr) return NULL;
    
    void* target = delegate->target;
    
    typedef void* (*DelegateMethod0)(void*);
    typedef void* (*DelegateMethod1)(void*, void*);
    typedef void* (*DelegateMethod2)(void*, void*, void*);
    typedef void* (*DelegateMethod3)(void*, void*, void*, void*);
    typedef void* (*DelegateMethod4)(void*, void*, void*, void*, void*);
    typedef void* (*DelegateMethod5)(void*, void*, void*, void*, void*, void*);
    typedef void* (*DelegateMethod6)(void*, void*, void*, void*, void*, void*, void*);
    typedef void* (*DelegateMethod7)(void*, void*, void*, void*, void*, void*, void*, void*);
    typedef void* (*DelegateMethod8)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
    
    switch (arg_count) {
        case 0: return ((DelegateMethod0)delegate->method_ptr)(target);
        case 1: return ((DelegateMethod1)delegate->method_ptr)(target, args[0]);
        case 2: return ((DelegateMethod2)delegate->method_ptr)(target, args[0], args[1]);
        case 3: return ((DelegateMethod3)delegate->method_ptr)(target, args[0], args[1], args[2]);
        case 4: return ((DelegateMethod4)delegate->method_ptr)(target, args[0], args[1], args[2], args[3]);
        case 5: return ((DelegateMethod5)delegate->method_ptr)(target, args[0], args[1], args[2], args[3], args[4]);
        case 6: return ((DelegateMethod6)delegate->method_ptr)(target, args[0], args[1], args[2], args[3], args[4], args[5]);
        case 7: return ((DelegateMethod7)delegate->method_ptr)(target, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        case 8: return ((DelegateMethod8)delegate->method_ptr)(target, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        default: return NULL;
    }
}

KRT_RUNTIME_EXPORT void KRT_API KrtFreeDelegate(void* delegate_handle) {
    if (!delegate_handle) return;
    KrtDelegate* delegate = (KrtDelegate*)delegate_handle;
    if (delegate->method_name) {
        KrtFree(delegate->method_name);
    }
    KrtFree(delegate);
}

#define GENERIC_STATIC_TABLE_SIZE 256

typedef struct GenericStaticEntry {
    char* key;
    void* value;
    struct GenericStaticEntry* next;
} GenericStaticEntry;

static GenericStaticEntry* g_generic_static_table[GENERIC_STATIC_TABLE_SIZE] = {NULL};

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % GENERIC_STATIC_TABLE_SIZE;
}

KRT_RUNTIME_EXPORT void* KRT_API KrtGetGenericStaticField(const char* mangled_name) {
    if (!mangled_name) return NULL;
    
    unsigned int hash = hash_string(mangled_name);
    GenericStaticEntry* entry = g_generic_static_table[hash];
    
    while (entry) {
        if (strcmp(entry->key, mangled_name) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

KRT_RUNTIME_EXPORT void KRT_API KrtSetGenericStaticField(const char* mangled_name, void* value) {
    if (!mangled_name) return;
    
    unsigned int hash = hash_string(mangled_name);
    GenericStaticEntry* entry = g_generic_static_table[hash];
    
    while (entry) {
        if (strcmp(entry->key, mangled_name) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    entry = (GenericStaticEntry*)KrtMalloc(sizeof(GenericStaticEntry));
    if (!entry) return;
    
    entry->key = KrtStrdup(mangled_name);
    entry->value = value;
    entry->next = g_generic_static_table[hash];
    g_generic_static_table[hash] = entry;
}

KRT_RUNTIME_EXPORT void KRT_API KrtClearGenericStaticFields(void) {
    for (int i = 0; i < GENERIC_STATIC_TABLE_SIZE; i++) {
        GenericStaticEntry* entry = g_generic_static_table[i];
        while (entry) {
            GenericStaticEntry* next = entry->next;
            if (entry->key) KrtFree(entry->key);
            KrtFree(entry);
            entry = next;
        }
        g_generic_static_table[i] = NULL;
    }
}

typedef struct KrtTypeMetadata {
    char* type_name;
    int is_class;
    int is_struct;
    int is_interface;
    char** interfaces;
    int interface_count;
    char* base_class;
} KrtTypeMetadata;

static KrtTypeMetadata** g_type_metadata = NULL;
static int g_type_metadata_count = 0;
static int g_type_metadata_capacity = 0;

KRT_RUNTIME_EXPORT int KRT_API KrtIsClass(void* obj) {
    if (!obj) return 0;
    // In a full implementation, this would check the object's type metadata
    // For now, assume all non-null pointers are reference types
    return 1;
}

KRT_RUNTIME_EXPORT int KRT_API KrtIsStruct(void* obj) {
    if (!obj) return 0;
    return 0;
}

KRT_RUNTIME_EXPORT int KRT_API KrtImplementsInterface(void* obj, const char* interface_name) {
    if (!obj || !interface_name) return 0;
    return 0;
}

KRT_RUNTIME_EXPORT int KRT_API KrtInheritsFrom(void* obj, const char* base_class_name) {
    if (!obj || !base_class_name) return 0;
    // In a full implementation, this would walk the inheritance hierarchy
    return 0;
}

KRT_RUNTIME_EXPORT int KRT_API KrtCheckGenericConstraint(void* obj, const char* constraint_type) {
    if (!obj || !constraint_type) return 0;
    
    if (strcmp(constraint_type, "class") == 0) {
        return KrtIsClass(obj);
    } else if (strcmp(constraint_type, "struct") == 0) {
        return KrtIsStruct(obj);
    } else {
        if (KrtImplementsInterface(obj, constraint_type)) return 1;
        if (KrtInheritsFrom(obj, constraint_type)) return 1;
    }
    return 0;
}