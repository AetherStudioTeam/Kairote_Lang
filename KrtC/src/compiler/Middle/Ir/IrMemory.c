#include "IrMemory.h"
#include <string.h>

#define MIN_ALIGNMENT 8

static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static KrtIRMemoryPool* create_pool(size_t size) {
    KrtIRMemoryPool* pool = (KrtIRMemoryPool*)KRT_MALLOC(sizeof(KrtIRMemoryPool));
    if (!pool) return NULL;
    
    pool->buffer = (char*)KRT_MALLOC(size);
    if (!pool->buffer) {
        KRT_FREE(pool);
        return NULL;
    }
    
    pool->size = size;
    pool->used = 0;
    pool->next = NULL;
    
    return pool;
}

static void destroy_pool(KrtIRMemoryPool* pool) {
    if (!pool) return;
    
    if (pool->buffer) {
        KRT_FREE(pool->buffer);
    }
    KRT_FREE(pool);
}

KrtIRMemoryArena* KrtIrArenaCreate(size_t pool_size) {
    KrtIRMemoryArena* arena = (KrtIRMemoryArena*)KRT_MALLOC(sizeof(KrtIRMemoryArena));
    if (!arena) return NULL;
    
    if (pool_size == 0) {
        pool_size = DEFAULT_POOL_SIZE;
    }
    
    arena->current_pool = create_pool(pool_size);
    if (!arena->current_pool) {
        KRT_FREE(arena);
        return NULL;
    }
    
    arena->pool_size = pool_size;
    arena->total_allocated = 0;
    arena->pool_count = 1;
    
    return arena;
}

void KrtIrArenaDestroy(KrtIRMemoryArena* arena) {
    if (!arena) return;
    
    KrtIRMemoryPool* pool = arena->current_pool;
    while (pool) {
        KrtIRMemoryPool* next = pool->next;
        destroy_pool(pool);
        pool = next;
    }
    
    KRT_FREE(arena);
}

void* KrtIrArenaAlloc(KrtIRMemoryArena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    size = align_size(size, MIN_ALIGNMENT);
    
    KrtIRMemoryPool* pool = arena->current_pool;
    
    if (pool->used + size > pool->size) {
        
        KrtIRMemoryPool* new_pool = create_pool(arena->pool_size > size ? arena->pool_size : size * 2);
        if (!new_pool) return NULL;
        
        new_pool->next = pool;
        arena->current_pool = new_pool;
        arena->pool_count++;
        pool = new_pool;
    }
    
    void* result = pool->buffer + pool->used;
    pool->used += size;
    arena->total_allocated += size;
    
    return result;
}

char* KrtIrArenaStrdup(KrtIRMemoryArena* arena, const char* str) {
    if (!arena || !str) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)KrtIrArenaAlloc(arena, len + 1);
    if (!result) return NULL;
    
    memcpy(result, str, len + 1);
    return result;
}

void KrtIrArenaReset(KrtIRMemoryArena* arena) {
    if (!arena) return;
    
    KrtIRMemoryPool* pool = arena->current_pool;
    while (pool) {
        pool->used = 0;
        pool = pool->next;
    }
    
    arena->total_allocated = 0;
}

void KrtIrArenaGetStats(KrtIRMemoryArena* arena, size_t* total_allocated, size_t* pool_count) {
    if (!arena) return;
    
    if (total_allocated) {
        *total_allocated = arena->total_allocated;
    }
    
    if (pool_count) {
        *pool_count = arena->pool_count;
    }
}