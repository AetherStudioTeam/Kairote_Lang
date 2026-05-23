#include "IrParamTable.h"
#include <string.h>

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

KrtIRParamTable* KrtIrParamTableCreate(KrtIRMemoryArena* arena, int initial_bucket_count) {
    if (!arena || initial_bucket_count <= 0) return NULL;
    
    int bucket_count = 1;
    while (bucket_count < initial_bucket_count) {
        bucket_count <<= 1;
    }
    
    KrtIRParamTable* table = (KrtIRParamTable*)KrtIrArenaAlloc(arena, sizeof(KrtIRParamTable));
    if (!table) return NULL;
    
    table->buckets = (KrtIRParamNode**)KrtIrArenaAlloc(arena, bucket_count * sizeof(KrtIRParamNode*));
    if (!table->buckets) return NULL;
    
    memset(table->buckets, 0, bucket_count * sizeof(KrtIRParamNode*));
    
    table->bucket_count = bucket_count;
    table->param_count = 0;
    table->arena = arena;
    
    return table;
}

bool KrtIrParamTableAdd(KrtIRParamTable* table, const char* name, KrtTokenType type, int index) {
    if (!table || !name) return false;
    
    if (KrtIrParamTableFind(table, name)) {
        return false; 
    }
    
    size_t hash = hash_string(name);
    int bucket_index = hash & (table->bucket_count - 1);
    
    KrtIRParamNode* node = (KrtIRParamNode*)KrtIrArenaAlloc(table->arena, sizeof(KrtIRParamNode));
    if (!node) return false;
    
    node->name = KrtIrArenaStrdup(table->arena, name);
    if (!node->name) return false;
    
    node->type = type;
    node->index = index;
    node->next = table->buckets[bucket_index];
    
    table->buckets[bucket_index] = node;
    table->param_count++;
    
    return true;
}

KrtIRParamNode* KrtIrParamTableFind(KrtIRParamTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    size_t hash = hash_string(name);
    int bucket_index = hash & (table->bucket_count - 1);
    
    KrtIRParamNode* node = table->buckets[bucket_index];
    while (node) {
        if (strcmp(node->name, name) == 0) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

int KrtIrParamTableCount(KrtIRParamTable* table) {
    return table ? table->param_count : 0;
}

void KrtIrParamTableForeach(KrtIRParamTable* table, void (*callback)(KrtIRParamNode* node, void* userdata), void* userdata) {
    if (!table || !callback) return;
    
    for (int i = 0; i < table->bucket_count; i++) {
        KrtIRParamNode* node = table->buckets[i];
        while (node) {
            callback(node, userdata);
            node = node->next;
        }
    }
}