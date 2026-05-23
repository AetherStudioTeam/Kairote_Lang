#include "Arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef KRT_ARENA_DEBUG
#define KRT_ARENA_DEBUG 0
#endif

static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

KrtArena* KrtArenaCreate(size_t block_size) {
    KrtArena* arena = (KrtArena*)KRT_MALLOC(sizeof(KrtArena));
    if (!arena) return NULL;
    
    memset(arena, 0, sizeof(KrtArena));
    arena->block_size = block_size > 0 ? block_size : KRT_ARENA_DEFAULT_BLOCK_SIZE;
    arena->blocks = NULL;
    arena->total_allocated = 0;
    arena->total_used = 0;
    arena->allocation_count = 0;
    arena->has_mutex = 0;
    
#ifdef _WIN32
    InitializeCriticalSection(&arena->mutex);
    arena->has_mutex = 1;
#else
    if (pthread_mutex_init(&arena->mutex, NULL) == 0) {
        arena->has_mutex = 1;
    }
#endif
    
    return arena;
}

void KrtArenaDestroy(KrtArena* arena) {
    if (!arena) return;
    
    KrtArenaBlock* current = arena->blocks;
    while (current) {
        KrtArenaBlock* next = current->next;
        KRT_FREE(current);
        current = next;
    }
    
    if (arena->has_mutex) {
#ifdef _WIN32
        DeleteCriticalSection(&arena->mutex);
#else
        pthread_mutex_destroy(&arena->mutex);
#endif
    }
    
    KRT_FREE(arena);
}

static KrtArenaBlock* KrtArenaCreateBlock(KrtArena* arena, size_t min_size) {
    size_t block_size = arena->block_size;
    if (min_size > block_size) {
        block_size = min_size;
    }
    
    KrtArenaBlock* block = (KrtArenaBlock*)KRT_MALLOC(sizeof(KrtArenaBlock) + block_size);
    if (!block) return NULL;
    
    block->next = NULL;
    block->size = block_size;
    block->used = 0;
    
    arena->total_allocated += sizeof(KrtArenaBlock) + block_size;
    
    return block;
}

void* KrtArenaAlloc(KrtArena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    size_t aligned_size = align_size(size, KRT_ARENA_ALIGNMENT);
    
    KRT_ARENA_LOCK(arena);
    
    KrtArenaBlock* block = arena->blocks;
    while (block) {
        if (block->used + aligned_size <= block->size) {
            void* ptr = block->data + block->used;
            block->used += aligned_size;
            arena->total_used += aligned_size;
            arena->allocation_count++;
            
            KRT_ARENA_UNLOCK(arena);
            return ptr;
        }
        block = block->next;
    }
    
    block = KrtArenaCreateBlock(arena, aligned_size);
    if (!block) {
        KRT_ARENA_UNLOCK(arena);
        return NULL;
    }
    
    block->next = arena->blocks;
    arena->blocks = block;
    
    void* ptr = block->data;
    block->used = aligned_size;
    arena->total_used += aligned_size;
    arena->allocation_count++;
    
    KRT_ARENA_UNLOCK(arena);
    return ptr;
}

void* KrtArenaCalloc(KrtArena* arena, size_t count, size_t size) {
    size_t total_size = count * size;
    void* ptr = KrtArenaAlloc(arena, total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* KrtArenaRealloc(KrtArena* arena, void* ptr, size_t old_size, size_t new_size) {
    if (!arena) return NULL;
    
    if (!ptr) {
        return KrtArenaAlloc(arena, new_size);
    }
    
    if (new_size <= old_size) {
        return ptr;
    }
    
    void* new_ptr = KrtArenaAlloc(arena, new_size);
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, old_size);
    }
    
    return new_ptr;
}

void KrtArenaReset(KrtArena* arena) {
    if (!arena) return;
    
    KRT_ARENA_LOCK(arena);
    
    KrtArenaBlock* current = arena->blocks;
    while (current) {
        current->used = 0;
        current = current->next;
    }
    
    arena->total_used = 0;
    arena->allocation_count = 0;
    
    KRT_ARENA_UNLOCK(arena);
}

void KrtArenaStats(KrtArena* arena) {
    if (!arena) return;
    
    KRT_ARENA_LOCK(arena);
    
    size_t block_count = 0;
    KrtArenaBlock* current = arena->blocks;
    while (current) {
        block_count++;
        current = current->next;
    }
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║              Arena Memory Statistics             ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Total Allocated:    %10zu bytes            ║\n", arena->total_allocated);
    printf("║ Total Used:         %10zu bytes            ║\n", arena->total_used);
    printf("║ Allocation Count:   %10zu                 ║\n", arena->allocation_count);
    printf("║ Block Count:        %10zu                 ║\n", block_count);
    printf("║ Block Size:         %10zu bytes            ║\n", arena->block_size);
    printf("║ Utilization:        %10.2f %%               ║\n", 
           arena->total_allocated > 0 ? (double)arena->total_used / arena->total_allocated * 100.0 : 0.0);
    printf("╚══════════════════════════════════════════════════╝\n");
    
    KRT_ARENA_UNLOCK(arena);
}

size_t KrtArenaGetUsage(KrtArena* arena) {
    if (!arena) return 0;
    
    KRT_ARENA_LOCK(arena);
    size_t usage = arena->total_used;
    KRT_ARENA_UNLOCK(arena);
    
    return usage;
}

size_t KrtArenaGetBlockCount(KrtArena* arena) {
    if (!arena) return 0;
    
    KRT_ARENA_LOCK(arena);
    
    size_t block_count = 0;
    KrtArenaBlock* current = arena->blocks;
    while (current) {
        block_count++;
        current = current->next;
    }
    
    KRT_ARENA_UNLOCK(arena);
    
    return block_count;
}