#ifndef KRT_IR_MEMORY_H
#define KRT_IR_MEMORY_H

#include "../../../Core/Utils/KrtCommon.h"

#define DEFAULT_POOL_SIZE (4 * 1024)

typedef struct KrtIRMemoryPool {
    char* buffer;           
    size_t size;            
    size_t used;            
    struct KrtIRMemoryPool* next;  
} KrtIRMemoryPool;

typedef struct KrtIRMemoryArena {
    KrtIRMemoryPool* current_pool;  
    size_t pool_size;               
    size_t total_allocated;         
    size_t pool_count;              
} KrtIRMemoryArena;

KrtIRMemoryArena* KrtIrArenaCreate(size_t pool_size);

void KrtIrArenaDestroy(KrtIRMemoryArena* arena);

void* KrtIrArenaAlloc(KrtIRMemoryArena* arena, size_t size);

char* KrtIrArenaStrdup(KrtIRMemoryArena* arena, const char* str);

void KrtIrArenaReset(KrtIRMemoryArena* arena);

void KrtIrArenaGetStats(KrtIRMemoryArena* arena, size_t* total_allocated, size_t* pool_count);

#endif 