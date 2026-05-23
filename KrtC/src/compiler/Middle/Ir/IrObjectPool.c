#include "Ir.h"
#include "IrType.h"
#include "IrSsa.h"
#include "IrObjectPool.h"
#include <string.h>
#include <stdio.h>

const size_t g_object_sizes[KRT_POOL_COUNT] = {
    sizeof(KrtIRInst),         
    sizeof(KrtIRBasicBlock),   
    sizeof(KrtIRValue),        
    sizeof(KrtIRType),         
    sizeof(KrtIRVarVersion),   
    sizeof(KrtIRPhi),          
};

const char* g_pool_names[KRT_POOL_COUNT] = {
    "Instruction ",
    "BasicBlock  ",
    "Value       ",
    "Type        ",
    "VarVersion  ",
    "Phi         ",
};

void pool_init(KrtIRObjectPool* pool, const char* name, size_t object_size, size_t block_size) {
    pool->name = name;
    pool->object_size = object_size;
    pool->block_size = block_size;
    pool->free_list = NULL;
    pool->blocks = NULL;
    pool->alloc_count = 0;
    pool->free_count = 0;
    pool->hit_count = 0;
    pool->miss_count = 0;
}

void* pool_alloc(KrtIRObjectPool* pool) {
    if (!pool) return NULL;
    
    pool->alloc_count++;
    
    if (pool->free_list) {
        KrtPoolNode* node = pool->free_list;
        pool->free_list = node->next;
        pool->hit_count++;
        
        memset(node->data, 0, pool->object_size);
        return node->data;
    }
    
    pool->miss_count++;
    return NULL;
}

void pool_grow(KrtIRObjectPool* pool, KrtIRMemoryArena* arena) {
    
    size_t block_mem_size = pool->block_size * (sizeof(KrtPoolNode) + pool->object_size);
    char* block_mem = (char*)KrtIrArenaAlloc(arena, block_mem_size);
    if (!block_mem) return;
    
    for (size_t i = 0; i < pool->block_size; i++) {
        KrtPoolNode* node = (KrtPoolNode*)(block_mem + i * (sizeof(KrtPoolNode) + pool->object_size));
        node->next = pool->free_list;
        pool->free_list = node;
    }
    
    KrtPoolNode* block_header = (KrtPoolNode*)KrtIrArenaAlloc(arena, sizeof(KrtPoolNode));
    if (block_header) {
        block_header->next = pool->blocks;
        pool->blocks = block_header;
    }
}

void KrtIrPoolManagerInit(KrtIRPoolManager* manager, KrtIRMemoryArena* arena) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(KrtIRPoolManager));
    manager->arena = arena;
    
    for (int i = 0; i < KRT_POOL_COUNT; i++) {
        pool_init(&manager->pools[i], g_pool_names[i], g_object_sizes[i], 64);
    }
}

void KrtIrPoolManagerDestroy(KrtIRPoolManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < KRT_POOL_COUNT; i++) {
        manager->pools[i].free_list = NULL;
        manager->pools[i].blocks = NULL;
    }
}

void* KrtIrPoolAlloc(KrtIRPoolManager* manager, KrtPoolObjectType type) {
    if (!manager || type < 0 || type >= KRT_POOL_COUNT) return NULL;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    pool->alloc_count++;
    
    if (pool->free_list) {
        KrtPoolNode* node = pool->free_list;
        pool->free_list = node->next;
        pool->hit_count++;
        
        memset(node->data, 0, pool->object_size);
        return node->data;
    }
    
    pool->miss_count++;
    pool_grow(pool, manager->arena);
    
    if (pool->free_list) {
        KrtPoolNode* node = pool->free_list;
        pool->free_list = node->next;
        memset(node->data, 0, pool->object_size);
        return node->data;
    }
    
    return NULL;
}

KrtIRInst* KrtIrPoolAllocInst(KrtIRPoolManager* manager) {
    return (KrtIRInst*)KrtIrPoolAlloc(manager, KRT_POOL_INST);
}

KrtIRBasicBlock* KrtIrPoolAllocBlock(KrtIRPoolManager* manager) {
    return (KrtIRBasicBlock*)KrtIrPoolAlloc(manager, KRT_POOL_BLOCK);
}

KrtIRType* KrtIrPoolAllocType(KrtIRPoolManager* manager) {
    return (KrtIRType*)KrtIrPoolAlloc(manager, KRT_POOL_TYPE);
}

KrtIRVarVersion* KrtIrPoolAllocVarVersion(KrtIRPoolManager* manager) {
    return (KrtIRVarVersion*)KrtIrPoolAlloc(manager, KRT_POOL_VAR_VERSION);
}

KrtIRPhi* KrtIrPoolAllocPhi(KrtIRPoolManager* manager) {
    return (KrtIRPhi*)KrtIrPoolAlloc(manager, KRT_POOL_PHI);
}

void KrtIrPoolFree(KrtIRPoolManager* manager, KrtPoolObjectType type, void* obj) {
    if (!manager || !obj || type < 0 || type >= KRT_POOL_COUNT) return;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    pool->free_count++;
    
    KrtPoolNode* node = (KrtPoolNode*)((char*)obj - sizeof(KrtPoolNode));
    
    node->next = pool->free_list;
    pool->free_list = node;
}

void KrtIrPoolFreeInst(KrtIRPoolManager* manager, KrtIRInst* inst) {
    KrtIrPoolFree(manager, KRT_POOL_INST, inst);
}

void KrtIrPoolFreeBlock(KrtIRPoolManager* manager, KrtIRBasicBlock* block) {
    KrtIrPoolFree(manager, KRT_POOL_BLOCK, block);
}

void KrtIrPoolFreeType(KrtIRPoolManager* manager, KrtIRType* type) {
    KrtIrPoolFree(manager, KRT_POOL_TYPE, type);
}

void KrtIrPoolFreeVarVersion(KrtIRPoolManager* manager, KrtIRVarVersion* var) {
    KrtIrPoolFree(manager, KRT_POOL_VAR_VERSION, var);
}

void KrtIrPoolFreePhi(KrtIRPoolManager* manager, KrtIRPhi* phi) {
    KrtIrPoolFree(manager, KRT_POOL_PHI, phi);
}

void KrtIrPoolClear(KrtIRPoolManager* manager, KrtPoolObjectType type) {
    if (!manager || type < 0 || type >= KRT_POOL_COUNT) return;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    
    pool->free_list = NULL;
    
}

void KrtIrPoolClearAll(KrtIRPoolManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < KRT_POOL_COUNT; i++) {
        KrtIrPoolClear(manager, (KrtPoolObjectType)i);
    }
}

static int count_pool_objects(KrtIRObjectPool* pool) {
    int count = 0;
    KrtPoolNode* node = pool->free_list;
    while (node) {
        count++;
        node = node->next;
    }
    return count;
}

void KrtIrPoolDefrag(KrtIRPoolManager* manager, KrtPoolObjectType type) {
    if (!manager || type < 0 || type >= KRT_POOL_COUNT) return;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    
    int free_count = count_pool_objects(pool);
    if (free_count < 2) return;  
    
    (void)free_count;
}

void KrtIrPoolDefragAll(KrtIRPoolManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < KRT_POOL_COUNT; i++) {
        KrtIrPoolDefrag(manager, (KrtPoolObjectType)i);
    }
}

int KrtIrPoolFragmentationRate(KrtIRPoolManager* manager, KrtPoolObjectType type) {
    if (!manager || type < 0 || type >= KRT_POOL_COUNT) return 0;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    
    int free_count = count_pool_objects(pool);
    int total_allocated = pool->alloc_count;
    
    if (total_allocated == 0) return 0;
    
    return (free_count * 100) / total_allocated;
}

int KrtIrPoolGetHitRate(KrtIRPoolManager* manager, KrtPoolObjectType type) {
    if (!manager || type < 0 || type >= KRT_POOL_COUNT) return 0;
    
    KrtIRObjectPool* pool = &manager->pools[type];
    int total = pool->hit_count + pool->miss_count;
    
    if (total == 0) return 0;
    return (pool->hit_count * 100) / total;
}

void KrtIrPoolPrintStats(KrtIRPoolManager* manager) {
    if (!manager) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              Object Pool Statistics                      ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║ Type         Allocs   Frees   Hit%%   Frag%%   Hit/Miss   ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    
    int total_allocs = 0;
    int total_frees = 0;
    int total_hits = 0;
    int total_misses = 0;
    
    for (int i = 0; i < KRT_POOL_COUNT; i++) {
        KrtIRObjectPool* pool = &manager->pools[i];
        if (pool->alloc_count > 0) {
            int hit_rate = KrtIrPoolGetHitRate(manager, (KrtPoolObjectType)i);
            int frag_rate = KrtIrPoolFragmentationRate(manager, (KrtPoolObjectType)i);
            printf("║ %s  %6d  %6d   %3d%%   %3d%%  %5d/%5d  ║\n",
                   pool->name,
                   pool->alloc_count,
                   pool->free_count,
                   hit_rate,
                   frag_rate,
                   pool->hit_count,
                   pool->miss_count);
            
            total_allocs += pool->alloc_count;
            total_frees += pool->free_count;
            total_hits += pool->hit_count;
            total_misses += pool->miss_count;
        }
    }
    
    printf("╠══════════════════════════════════════════════════════════╣\n");
    int total_rate = (total_hits + total_misses > 0) 
        ? (total_hits * 100) / (total_hits + total_misses) 
        : 0;
    printf("║ TOTAL        %6d  %6d   %3d%%                      ║\n",
           total_allocs, total_frees, total_rate);
    printf("╚══════════════════════════════════════════════════════════╝\n");
}