#ifndef KRT_IR_OBJECT_POOL_H
#define KRT_IR_OBJECT_POOL_H

#include "IrMemory.h"

typedef struct KrtIRInst KrtIRInst;
typedef struct KrtIRBasicBlock KrtIRBasicBlock;
typedef struct KrtIRFunction KrtIRFunction;
typedef struct KrtIRType KrtIRType;
typedef struct KrtIRVarVersion KrtIRVarVersion;
typedef struct KrtIRPhi KrtIRPhi;

typedef enum {
    KRT_POOL_INST,         
    KRT_POOL_BLOCK,        
    KRT_POOL_VALUE,        
    KRT_POOL_TYPE,         
    KRT_POOL_VAR_VERSION,  
    KRT_POOL_PHI,          
    KRT_POOL_COUNT
} KrtPoolObjectType;

typedef struct KrtPoolNode {
    struct KrtPoolNode* next;
    char data[];  
} KrtPoolNode;

typedef struct {
    const char* name;          
    size_t object_size;        
    size_t block_size;         
    
    KrtPoolNode* free_list;     
    KrtPoolNode* blocks;        
    
    int alloc_count;           
    int free_count;            
    int hit_count;             
    int miss_count;            
} KrtIRObjectPool;

typedef struct {
    KrtIRObjectPool pools[KRT_POOL_COUNT];
    KrtIRMemoryArena* arena;    
} KrtIRPoolManager;

void KrtIrPoolManagerInit(KrtIRPoolManager* manager, KrtIRMemoryArena* arena);
void KrtIrPoolManagerDestroy(KrtIRPoolManager* manager);

void* KrtIrPoolAlloc(KrtIRPoolManager* manager, KrtPoolObjectType type);
KrtIRInst* KrtIrPoolAllocInst(KrtIRPoolManager* manager);
KrtIRBasicBlock* KrtIrPoolAllocBlock(KrtIRPoolManager* manager);
KrtIRType* KrtIrPoolAllocType(KrtIRPoolManager* manager);
KrtIRVarVersion* KrtIrPoolAllocVarVersion(KrtIRPoolManager* manager);
KrtIRPhi* KrtIrPoolAllocPhi(KrtIRPoolManager* manager);

void KrtIrPoolFree(KrtIRPoolManager* manager, KrtPoolObjectType type, void* obj);
void KrtIrPoolFreeInst(KrtIRPoolManager* manager, KrtIRInst* inst);
void KrtIrPoolFreeBlock(KrtIRPoolManager* manager, KrtIRBasicBlock* block);
void KrtIrPoolFreeType(KrtIRPoolManager* manager, KrtIRType* type);
void KrtIrPoolFreeVarVersion(KrtIRPoolManager* manager, KrtIRVarVersion* var);
void KrtIrPoolFreePhi(KrtIRPoolManager* manager, KrtIRPhi* phi);

void KrtIrPoolClear(KrtIRPoolManager* manager, KrtPoolObjectType type);
void KrtIrPoolClearAll(KrtIRPoolManager* manager);

void KrtIrPoolDefrag(KrtIRPoolManager* manager, KrtPoolObjectType type);
void KrtIrPoolDefragAll(KrtIRPoolManager* manager);

int KrtIrPoolFragmentationRate(KrtIRPoolManager* manager, KrtPoolObjectType type);

void KrtIrPoolPrintStats(KrtIRPoolManager* manager);
int KrtIrPoolGetHitRate(KrtIRPoolManager* manager, KrtPoolObjectType type);

void pool_init(KrtIRObjectPool* pool, const char* name, size_t object_size, size_t block_size);
void* pool_alloc(KrtIRObjectPool* pool);
void pool_grow(KrtIRObjectPool* pool, KrtIRMemoryArena* arena);

#endif 