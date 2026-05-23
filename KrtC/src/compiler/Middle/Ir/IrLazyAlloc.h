#ifndef KRT_IR_LAZY_ALLOC_H
#define KRT_IR_LAZY_ALLOC_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    KRT_LAZY_OP_ALLOC,       
    KRT_LAZY_OP_COPY,        
    KRT_LAZY_OP_INIT,        
    KRT_LAZY_OP_COUNT
} KrtLazyOpType;

typedef struct KrtLazyOp {
    KrtLazyOpType type;
    void* target;           
    void* source;           
    size_t size;            
    int processed;          
    struct KrtLazyOp* next;
} KrtLazyOp;

typedef struct {
    KrtLazyOp* pending;      
    KrtLazyOp* free_list;    
    int pending_count;      
    int processed_count;    
    int coalesced_count;    
    
    int batch_size;         
    int enable_coalesce;    
} KrtLazyAllocManager;

void KrtIrLazyInit(KrtLazyAllocManager* manager);
void KrtIrLazyDestroy(KrtLazyAllocManager* manager);

void KrtIrLazyRecordAlloc(KrtLazyAllocManager* manager, void** target, size_t size);
void KrtIrLazyRecordCopy(KrtLazyAllocManager* manager, void** target, void* source, size_t size);
void KrtIrLazyRecordInit(KrtLazyAllocManager* manager, void* target, size_t size);

void KrtIrLazyFlush(KrtLazyAllocManager* manager);
void KrtIrLazyFlushBatch(KrtLazyAllocManager* manager, int max_ops);

void KrtIrLazyCoalesceAllocs(KrtLazyAllocManager* manager);

bool KrtIrLazyHasPending(KrtLazyAllocManager* manager);
int KrtIrLazyPendingCount(KrtLazyAllocManager* manager);

void KrtIrLazyPrintStats(KrtLazyAllocManager* manager);

#endif 