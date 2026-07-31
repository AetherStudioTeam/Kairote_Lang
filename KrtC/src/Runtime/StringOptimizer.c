
#include "StringOptimizer.h"
#include "../Core/Utils/KrtString.h"
#include "../Core/Utils/KrtCommon.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations for runtime functions used by StringOptimizer */
extern void* KrtMalloc(size_t size);
extern void KrtFree(void* ptr);
extern char* KrtStrdup(const char* src);

typedef struct StringConstant {
    char* str;
    size_t len;
    size_t hash;
    struct StringConstant* next;
} StringConstant;

#define STRING_POOL_SIZE 256
static StringConstant* string_pool[STRING_POOL_SIZE];
static int string_pool_initialized = 0;

static size_t string_hash(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static void init_string_pool() {
    if (string_pool_initialized) return;
    
    for (int i = 0; i < STRING_POOL_SIZE; i++) {
        string_pool[i] = NULL;
    }
    string_pool_initialized = 1;
}

static StringConstant* find_string_in_pool(const char* str) {
    if (!str) return NULL;
    
    size_t hash = string_hash(str);
    size_t index = hash % STRING_POOL_SIZE;
    
    StringConstant* current = string_pool[index];
    while (current) {
        if (current->hash == hash && strcmp(current->str, str) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static StringConstant* add_string_to_pool(const char* str) {
    if (!str) return NULL;
    
    init_string_pool();
    
    size_t hash = string_hash(str);
    size_t index = hash % STRING_POOL_SIZE;
    
    StringConstant* new_const = KrtMalloc(sizeof(StringConstant));
    if (!new_const) return NULL;
    
    size_t len = strlen(str);
    new_const->str = KrtMalloc(len + 1);
    if (!new_const->str) {
        KrtFree(new_const);
        return NULL;
    }
    
    strcpy(new_const->str, str);
    new_const->len = len;
    new_const->hash = hash;
    new_const->next = string_pool[index];
    
    string_pool[index] = new_const;
    
    return new_const;
}

char* KrtStrcatOptimized(const char* str1, const char* str2) {
    if (!str1 && !str2) return NULL;
    if (!str1) return KrtStrdup(str2);
    if (!str2) return KrtStrdup(str1);
    
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t total_len = len1 + len2 + 1;
    
    char* result = KrtMalloc(total_len);
    if (!result) return NULL;
    
    memcpy(result, str1, len1);
    memcpy(result + len1, str2, len2 + 1); 
    
    return result;
}

char* KrtStrcatMultiple(const char** parts, int count) {
    if (!parts || count <= 0) return NULL;
    
    size_t total_len = 1; 
    for (int i = 0; i < count; i++) {
        if (parts[i]) {
            total_len += strlen(parts[i]);
        }
    }
    
    char* result = KrtMalloc(total_len);
    if (!result) return NULL;
    
    char* current = result;
    for (int i = 0; i < count; i++) {
        if (parts[i]) {
            size_t part_len = strlen(parts[i]);
            memcpy(current, parts[i], part_len);
            current += part_len;
        }
    }
    *current = '\0';
    
    return result;
}

const char* KrtGetStringConstant(const char* str) {
    if (!str) return NULL;
    
    StringConstant* found = find_string_in_pool(str);
    if (found) {
        return found->str;
    }
    
    StringConstant* new_const = add_string_to_pool(str);
    return new_const ? new_const->str : str;
}

char* KrtIntToStringOptimized(int num) {
    
    char* buffer = KrtMalloc(12); 
    if (!buffer) return NULL;
    
    int i = 10; 
    int negative = 0;
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    buffer[11] = '\0';
    
    if (num == 0) {
        buffer[10] = '0';
        i = 9;
    } else {
        
        while (num > 0) {
            buffer[i--] = (num % 10) + '0';
            num /= 10;
        }
    }
    
    if (negative) {
        buffer[i--] = '-';
    }
    
    char* result = buffer + (i + 1);
    
    if (result != buffer) {
        size_t len = 12 - (result - buffer);
        memmove(buffer, result, len);
        result = buffer;
    }
    
    return result;
}

char* KrtDoubleToStringOptimized(double num) {
    
    char* buffer = KrtMalloc(32);
    if (!buffer) return NULL;
    
    int len = snprintf(buffer, 32, "%.6g", num);
    
    if (len < 0 || len >= 32) {
        KrtFree(buffer);
        return NULL;
    }
    
    return buffer;
}

int KrtStringFastCompare(const char* str1, const char* str2) {
    if (str1 == str2) return 0;
    if (!str1 || !str2) return str1 ? 1 : -1;
    
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

void KrtProcessStringsBatch(const char** strings, int count, void (*processor)(const char*)) {
    if (!strings || !processor || count <= 0) return;
    
    for (int i = 0; i < count; i++) {
        if (strings[i]) {
            processor(strings[i]);
        }
    }
}

typedef struct StringPool {
    char* buffer;
    size_t size;
    size_t used;
    struct StringPool* next;
} StringPool;

#define STRING_POOL_CHUNK_SIZE 4096

static StringPool* string_memory_pool = NULL;

char* KrtPoolAllocString(size_t size) {
    if (size > STRING_POOL_CHUNK_SIZE / 4) {
        
        return KrtMalloc(size);
    }
    
    StringPool* pool = string_memory_pool;
    while (pool) {
        if (pool->used + size <= pool->size) {
            char* result = pool->buffer + pool->used;
            pool->used += size;
            return result;
        }
        pool = pool->next;
    }
    
    pool = KrtMalloc(sizeof(StringPool));
    if (!pool) return NULL;
    
    pool->buffer = KrtMalloc(STRING_POOL_CHUNK_SIZE);
    if (!pool->buffer) {
        KrtFree(pool);
        return NULL;
    }
    
    pool->size = STRING_POOL_CHUNK_SIZE;
    pool->used = size;
    pool->next = string_memory_pool;
    string_memory_pool = pool;
    
    return pool->buffer;
}