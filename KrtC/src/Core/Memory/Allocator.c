#include "Allocator.h"
#include "../Utils/KrtCommon.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define MEMORY_MAGIC 0xDEADBEEF
#define CANARY_VALUE 0xCAFEBABE
#define POISON_VALUE 0xDD
#define FREED_MAGIC  0xFREEDEAD

MemorySafetyManager* g_memory_safety = NULL;

static void add_memory_block(MemoryBlock* block) {
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    block->next = g_memory_safety->blocks;
    block->prev = NULL;
    if (g_memory_safety->blocks) {
        g_memory_safety->blocks->prev = block;
    }
    g_memory_safety->blocks = block;
    g_memory_safety->block_count++;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}

static void remove_memory_block(MemoryBlock* block) {
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        g_memory_safety->blocks = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    g_memory_safety->block_count--;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}

void KrtMemorySafetyInit(void) {
    if (g_memory_safety) return;
    
    g_memory_safety = KRT_MALLOC(sizeof(MemorySafetyManager));
    if (!g_memory_safety) {
        exit(1);
    }
    
    g_memory_safety->blocks = NULL;
    g_memory_safety->block_count = 0;
    g_memory_safety->poison_enabled = true;
    g_memory_safety->canary_enabled = true;
    
#ifdef _WIN32
    InitializeCriticalSection(&g_memory_safety->mutex);
#else
    if (pthread_mutex_init(&g_memory_safety->mutex, NULL) != 0) {
        KRT_FREE(g_memory_safety);
        g_memory_safety = NULL;
        exit(1);
    }
#endif

}

void KrtMemorySafetyCleanup(void) {
    if (!g_memory_safety) return;
    
    if (g_memory_safety->block_count > 0) {
        KrtMemoryDumpBlocks();
    }
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        MemoryBlock* next = current->next;
        
        if (!current->is_freed) {
        }
        
        free(current->actual_ptr);
        free(current);
        current = next;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_destroy(&g_memory_safety->mutex);
#endif
    free(g_memory_safety);
    g_memory_safety = NULL;
    
}

void* KrtSafeMalloc(size_t size, const char* file, int line) {
    if (!g_memory_safety) {
        KrtMemorySafetyInit();
    }
    
    if (size == 0) return NULL;
    
    size_t actual_size = size;
    if (g_memory_safety->canary_enabled) {
        actual_size += 2 * sizeof(uint32_t);  
    }
    
    void* actual_ptr = malloc(actual_size);
    if (!actual_ptr) {
        KrtMemoryReportError("MALLOC_FAILED", NULL, file, line, 
                            "Failed to allocate %zu bytes", size);
        return NULL;
    }
    
    MemoryBlock* block = malloc(sizeof(MemoryBlock));
    if (!block) {
        free(actual_ptr);
        return NULL;
    }
    
    void* user_ptr = actual_ptr;
    if (g_memory_safety->canary_enabled) {
        user_ptr = (char*)actual_ptr + sizeof(uint32_t);  
    }
    
    block->actual_ptr = actual_ptr;
    block->user_ptr = user_ptr;
    block->user_size = size;
    block->actual_size = actual_size;
    block->magic = MEMORY_MAGIC;
    block->protection = MEM_PROTECT_READ | MEM_PROTECT_WRITE;
    block->is_freed = false;
    block->file = file;
    block->line = line;
    
    if (g_memory_safety->canary_enabled) {
        KrtSetCanary(block);
    }
    
    add_memory_block(block);

    return user_ptr;
}

void* KrtSafeCalloc(size_t count, size_t size, const char* file, int line) {
    if (count == 0 || size == 0) return NULL;
    
    void* ptr = KrtSafeMalloc(count * size, file, line);
    if (ptr) {
        memset(ptr, 0, count * size);
    }
    return ptr;
}

void* KrtSafeRealloc(void* old_ptr, size_t new_size, const char* file, int line) {
    if (!old_ptr) {
        return KrtSafeMalloc(new_size, file, line);
    }
    
    if (new_size == 0) {
        KrtSafeFree(old_ptr, file, line);
        return NULL;
    }
    
    MemoryBlock* old_block = KrtPtrGetBlock(old_ptr);
    if (!old_block) {
        KrtMemoryReportError("INVALID_REALLOC", old_ptr, file, line,
                            "Attempt to realloc untracked pointer %p", old_ptr);
        return NULL;
    }
    
    void* new_ptr = KrtSafeMalloc(new_size, file, line);
    if (!new_ptr) {
        return NULL;
    }
    
    size_t copy_size = (old_block->user_size < new_size) ? old_block->user_size : new_size;
    memcpy(new_ptr, old_ptr, copy_size);
    
    if (new_size > copy_size) {
        memset((char*)new_ptr + copy_size, 0, new_size - copy_size);
    }
    
    KrtSafeFree(old_ptr, file, line);
    
    return new_ptr;
}

char* KrtSafeStrdup(const char* str, const char* file, int line) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* copy = (char*)KrtSafeMalloc(len + 1, file, line);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

void KrtSafeFree(void* ptr, const char* file, int line) {
    if (!ptr) return;
    
    if (KrtPtrCheckDoubleFree(ptr, file, line)) {
        return;
    }
    
    MemoryBlock* block = KrtPtrGetBlock(ptr);
    if (!block) {
        KrtMemoryReportError("INVALID_FREE", ptr, file, line,
                            "Attempt to free untracked pointer %p", ptr);
        return;
    }
    
    if (!KrtCheckCanary(block)) {
        KrtMemoryReportError("CANARY_CORRUPTED", ptr, file, line,
                            "Memory canary corrupted for block %p", ptr);
    }
    
    if (KrtMemoryIsPoisoned(ptr, block->user_size)) {
        KrtMemoryReportError("USE_AFTER_FREE", ptr, file, line,
                            "Use after free detected for block %p", ptr);
    }
    
    block->is_freed = true;
    
    if (g_memory_safety->poison_enabled) {
        KrtMemoryPoison(ptr, block->user_size);
    }
    
    remove_memory_block(block);

    free(block->actual_ptr);
    free(block);
}

bool KrtBoundsCheck(const void* array, size_t index, size_t element_size, size_t array_size) {
    if (!array || element_size == 0) return false;
    
    if (index >= array_size) {
        return false;
    }
    
    return true;
}

bool KrtBufferCheck(const void* buffer, size_t offset, size_t size, size_t buffer_size) {
    if (!buffer || size == 0) return false;
    
    if (offset + size > buffer_size) {
        return false;
    }
    
    return true;
}

bool KrtPtrCheckDoubleFree(const void* ptr, const char* file, int line) {
    if (!ptr) return false;
    
    MemoryBlock* block = KrtPtrGetBlock(ptr);
    if (block && block->is_freed) {
        KrtMemoryReportError("DOUBLE_FREE", ptr, file, line,
                            "Double free detected for block %p", ptr);
        return true;
    }
    
    return false;
}

MemoryBlock* KrtPtrGetBlock(const void* ptr) {
    if (!g_memory_safety || !ptr) return NULL;
    
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        if (current->user_ptr == ptr) {
#ifdef _WIN32
            LeaveCriticalSection(&g_memory_safety->mutex);
#else
            pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
            return current;
        }
        current = current->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
    return NULL;
}

bool KrtMemoryPtrIsValid(const void* ptr) {
    if (!ptr) return false;
    
    MemoryBlock* block = KrtPtrGetBlock(ptr);
    if (!block) return false;
    
    if (block->magic != MEMORY_MAGIC) return false;
    if (block->is_freed) return false;
    if (!KrtCheckCanary(block)) return false;
    
    return true;
}

void KrtSetCanary(MemoryBlock* block) {
    if (!block || !g_memory_safety->canary_enabled) return;
    
    uint32_t* front_canary = (uint32_t*)block->actual_ptr;
    uint32_t* back_canary = (uint32_t*)((char*)block->user_ptr + block->user_size);
    
    *front_canary = CANARY_VALUE;
    *back_canary = CANARY_VALUE;
}

bool KrtCheckCanary(const MemoryBlock* block) {
    if (!block || !g_memory_safety->canary_enabled) return true;
    
    uint32_t front_canary = *(uint32_t*)block->actual_ptr;
    uint32_t back_canary = *(uint32_t*)((char*)block->user_ptr + block->user_size);
    
    return front_canary == CANARY_VALUE && back_canary == CANARY_VALUE;
}

void KrtMemoryPoison(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    memset(ptr, POISON_VALUE, size);
}

bool KrtMemoryIsPoisoned(const void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    
    const unsigned char* bytes = (const unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != POISON_VALUE) return false;
    }
    return true;
}

void KrtMemoryReportError(const char* error_type, const void* ptr, 
                           const char* file, int line, const char* format, ...) {
    (void)error_type;
    (void)ptr;
    (void)file;
    (void)line;
    (void)format;
    #ifdef KRT_DEBUG
    __debugbreak();
    #endif
}

void KrtMemoryDumpBlocks(void) {
    if (!g_memory_safety) return;

#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif

    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        current = current->next;
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
}

size_t KrtMemoryGetTotalUsage(void) {
    if (!g_memory_safety) return 0;
    
    size_t total = 0;
#ifdef _WIN32
    EnterCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_lock(&g_memory_safety->mutex);
#endif
    
    MemoryBlock* current = g_memory_safety->blocks;
    while (current) {
        if (!current->is_freed) {
            total += current->user_size;
        }
        current = current->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&g_memory_safety->mutex);
#else
    pthread_mutex_unlock(&g_memory_safety->mutex);
#endif
    return total;
}

size_t KrtMemoryGetBlockCount(void) {
    return g_memory_safety ? g_memory_safety->block_count : 0;
}

void KrtMemoryDumpStats(void) {
}