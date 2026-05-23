#include "IrLazyAlloc.h"
#include "../../../Core/Utils/KrtCommon.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static KrtLazyOp* create_op(KrtLazyAllocManager* manager) {
    if (!manager) return NULL;
    
    if (manager->free_list) {
        KrtLazyOp* op = manager->free_list;
        manager->free_list = op->next;
        memset(op, 0, sizeof(KrtLazyOp));
        return op;
    }
    
    return (KrtLazyOp*)KRT_CALLOC(1, sizeof(KrtLazyOp));
}

static void recycle_op(KrtLazyAllocManager* manager, KrtLazyOp* op) {
    if (!manager || !op) return;
    
    op->next = manager->free_list;
    manager->free_list = op;
}

void KrtIrLazyInit(KrtLazyAllocManager* manager) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(KrtLazyAllocManager));
    manager->batch_size = 32;      
    manager->enable_coalesce = 1;  
}

void KrtIrLazyDestroy(KrtLazyAllocManager* manager) {
    if (!manager) return;
    
    KrtIrLazyFlush(manager);
    
    KrtLazyOp* op = manager->pending;
    while (op) {
        KrtLazyOp* next = op->next;
        KRT_FREE(op);
        op = next;
    }
    
    op = manager->free_list;
    while (op) {
        KrtLazyOp* next = op->next;
        KRT_FREE(op);
        op = next;
    }
    
    memset(manager, 0, sizeof(KrtLazyAllocManager));
}

void KrtIrLazyRecordAlloc(KrtLazyAllocManager* manager, void** target, size_t size) {
    if (!manager || !target) return;
    
    KrtLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = KRT_LAZY_OP_ALLOC;
    op->target = target;
    op->size = size;
    op->processed = 0;
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}

void KrtIrLazyRecordCopy(KrtLazyAllocManager* manager, void** target, void* source, size_t size) {
    if (!manager || !target) return;
    
    KrtLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = KRT_LAZY_OP_COPY;
    op->target = target;
    op->source = source;
    op->size = size;
    op->processed = 0;
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}

void KrtIrLazyRecordInit(KrtLazyAllocManager* manager, void* target, size_t size) {
    if (!manager || !target) return;
    
    KrtLazyOp* op = create_op(manager);
    if (!op) return;
    
    op->type = KRT_LAZY_OP_INIT;
    op->target = target;
    op->size = size;
    op->processed = 0;
    
    op->next = manager->pending;
    manager->pending = op;
    manager->pending_count++;
}

static void execute_op(KrtLazyOp* op) {
    if (!op || op->processed) return;
    
    switch (op->type) {
        case KRT_LAZY_OP_ALLOC: {
            void** target = (void**)op->target;
            if (target && *target == NULL) {
                *target = KRT_MALLOC(op->size);
                if (*target) {
                    memset(*target, 0, op->size);
                }
            }
            break;
        }
        
        case KRT_LAZY_OP_COPY: {
            void** target = (void**)op->target;
            if (target && op->source && op->size > 0) {
                if (*target == NULL) {
                    *target = KRT_MALLOC(op->size);
                }
                if (*target) {
                    memcpy(*target, op->source, op->size);
                }
            }
            break;
        }
        
        case KRT_LAZY_OP_INIT: {
            void* target = op->target;
            if (target && op->size > 0) {
                memset(target, 0, op->size);
            }
            break;
        }
        
        default:
            break;
    }
    
    op->processed = 1;
}

void KrtIrLazyFlush(KrtLazyAllocManager* manager) {
    if (!manager) return;
    
    if (manager->enable_coalesce) {
        KrtIrLazyCoalesceAllocs(manager);
    }
    
    KrtLazyOp* op = manager->pending;
    KrtLazyOp* processed_tail = NULL;
    
    while (op) {
        KrtLazyOp* next = op->next;
        
        if (!op->processed) {
            execute_op(op);
            manager->processed_count++;
        }
        
        if (op->processed) {
            if (processed_tail) {
                processed_tail->next = op;
            }
            processed_tail = op;
            op->next = NULL;
        }
        
        op = next;
    }
    
    op = processed_tail;
    while (op) {
        KrtLazyOp* next = op->next;
        recycle_op(manager, op);
        manager->pending_count--;
        op = next;
    }
    
    manager->pending = NULL;
}

void KrtIrLazyFlushBatch(KrtLazyAllocManager* manager, int max_ops) {
    if (!manager || max_ops <= 0) return;
    
    int count = 0;
    KrtLazyOp* op = manager->pending;
    
    while (op && count < max_ops) {
        if (!op->processed) {
            execute_op(op);
            manager->processed_count++;
            count++;
        }
        op = op->next;
    }
}

void KrtIrLazyCoalesceAllocs(KrtLazyAllocManager* manager) {
    if (!manager || !manager->enable_coalesce) return;
    
    size_t size_histogram[8] = {0};  
    
    KrtLazyOp* op = manager->pending;
    while (op) {
        if (op->type == KRT_LAZY_OP_ALLOC && !op->processed) {
            
            int bucket = 0;
            size_t size = op->size;
            while (size > 8 && bucket < 7) {
                size >>= 1;
                bucket++;
            }
            size_histogram[bucket]++;
        }
        op = op->next;
    }
    
    int coalesced = 0;
    for (int i = 0; i < 8; i++) {
        if (size_histogram[i] > 4) {
            
            coalesced++;
        }
    }
    
    manager->coalesced_count += coalesced;
}

bool KrtIrLazyHasPending(KrtLazyAllocManager* manager) {
    if (!manager) return false;
    
    KrtLazyOp* op = manager->pending;
    while (op) {
        if (!op->processed) return true;
        op = op->next;
    }
    return false;
}

int KrtIrLazyPendingCount(KrtLazyAllocManager* manager) {
    if (!manager) return 0;
    return manager->pending_count;
}

void KrtIrLazyPrintStats(KrtLazyAllocManager* manager) {
    if (!manager) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         Lazy Allocation Statistics               ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Pending Operations:     %10d               ║\n", manager->pending_count);
    printf("║ Processed Operations:   %10d               ║\n", manager->processed_count);
    printf("║ Coalesced Groups:       %10d               ║\n", manager->coalesced_count);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Batch Size:             %10d               ║\n", manager->batch_size);
    printf("║ Coalesce Enabled:       %10s               ║\n", 
           manager->enable_coalesce ? "Yes" : "No");
    printf("╚══════════════════════════════════════════════════╝\n");
}