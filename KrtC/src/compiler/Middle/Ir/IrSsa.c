#include "IrSsa.h"
#include <string.h>
#include <stdio.h>

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

KrtIRVarTable* KrtIrVarTableCreate(KrtIRMemoryArena* arena, int bucket_count) {
    if (!arena) return NULL;
    
    KrtIRVarTable* table = (KrtIRVarTable*)KrtIrArenaAlloc(arena, sizeof(KrtIRVarTable));
    if (!table) return NULL;
    
    table->buckets = (KrtIRVarVersion**)KrtIrArenaAlloc(arena, bucket_count * sizeof(KrtIRVarVersion*));
    if (!table->buckets) return NULL;
    
    memset(table->buckets, 0, bucket_count * sizeof(KrtIRVarVersion*));
    table->bucket_count = bucket_count;
    table->var_count = 0;
    
    return table;
}

void KrtIrVarTableDestroy(KrtIRVarTable* table) {
    
    (void)table;
}

static KrtIRVarVersion* var_table_find(KrtIRVarTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    unsigned int hash = hash_string(name);
    int index = hash % table->bucket_count;
    
    KrtIRVarVersion* var = table->buckets[index];
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    
    return NULL;
}

static void var_table_add(KrtIRVarTable* table, KrtIRVarVersion* var) {
    if (!table || !var || !var->name) return;
    
    unsigned int hash = hash_string(var->name);
    int index = hash % table->bucket_count;
    
    var->next = table->buckets[index];
    table->buckets[index] = var;
    table->var_count++;
}

KrtIRVarVersion* KrtIrVarGetVersion(KrtIRVarTable* table, const char* name) {
    return var_table_find(table, name);
}

KrtIRVarVersion* KrtIrVarNewVersion(KrtIRVarTable* table, const char* name, KrtIRType* type,
                                       KrtIRBasicBlock* block, KrtIRInst* def) {
    if (!table || !name) return NULL;
    
    KrtIRVarVersion* existing = var_table_find(table, name);
    int new_version = 0;
    if (existing) {
        new_version = existing->version + 1;
    }
    
    KrtIRVarVersion* var = (KrtIRVarVersion*)KRT_CALLOC(1, sizeof(KrtIRVarVersion));
    if (!var) return NULL;
    
    var->name = KRT_STRDUP(name);
    var->version = new_version;
    var->type = type;
    var->block = block;
    var->def = def;
    
    var_table_add(table, var);
    
    return var;
}

KrtIRVarVersion* KrtIrVarFindVersion(KrtIRVarTable* table, const char* name, KrtIRBasicBlock* block) {
    
    (void)block;
    return var_table_find(table, name);
}

char* KrtIrVarVersionedName(KrtIRMemoryArena* arena, const char* name, int version) {
    if (!arena || !name) return NULL;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s_%d", name, version);
    return KrtIrArenaStrdup(arena, buffer);
}

KrtIRPhi* KrtIrPhiCreate(KrtIRMemoryArena* arena, const char* var_name, KrtIRType* type,
                           int pred_count, KrtIRBasicBlock* parent) {
    if (!arena || !var_name) return NULL;
    
    KrtIRPhi* phi = (KrtIRPhi*)KrtIrArenaAlloc(arena, sizeof(KrtIRPhi));
    if (!phi) return NULL;
    
    phi->var_name = KrtIrArenaStrdup(arena, var_name);
    phi->type = type;
    phi->pred_count = pred_count;
    phi->parent_block = parent;
    phi->next = NULL;
    
    if (pred_count > 0) {
        phi->blocks = (KrtIRBasicBlock**)KrtIrArenaAlloc(arena, pred_count * sizeof(KrtIRBasicBlock*));
        phi->versions = (KrtIRVarVersion**)KrtIrArenaAlloc(arena, pred_count * sizeof(KrtIRVarVersion*));
        
        memset(phi->blocks, 0, pred_count * sizeof(KrtIRBasicBlock*));
        memset(phi->versions, 0, pred_count * sizeof(KrtIRVarVersion*));
    } else {
        phi->blocks = NULL;
        phi->versions = NULL;
    }
    
    return phi;
}

void KrtIrPhiAddOperand(KrtIRPhi* phi, KrtIRBasicBlock* block, KrtIRVarVersion* version, int index) {
    if (!phi || index < 0 || index >= phi->pred_count) return;
    
    phi->blocks[index] = block;
    phi->versions[index] = version;
}

KrtIRPhi* KrtIrPhiFind(KrtIRBasicBlock* block, const char* var_name) {
    if (!block || !var_name) return NULL;
    
    (void)block;
    (void)var_name;
    return NULL;
}

void KrtIrBlockAddPhi(KrtIRBasicBlock* block, KrtIRPhi* phi) {
    if (!block || !phi) return;
    
    phi->next = NULL;  
    (void)block;
}

KrtIRSSABuilder* KrtIrSsaBuilderCreate(KrtIRBuilder* builder) {
    if (!builder) return NULL;
    
    KrtIRSSABuilder* ssa = (KrtIRSSABuilder*)KRT_CALLOC(1, sizeof(KrtIRSSABuilder));
    if (!ssa) return NULL;
    
    ssa->builder = builder;
    ssa->arena = builder->arena;
    ssa->var_table = KrtIrVarTableCreate(ssa->arena, 64);
    ssa->version_counters = NULL;
    ssa->var_capacity = 0;
    
    return ssa;
}

void KrtIrSsaBuilderDestroy(KrtIRSSABuilder* ssa_builder) {
    if (!ssa_builder) return;
    
    if (ssa_builder->var_table) {
        KrtIrVarTableDestroy(ssa_builder->var_table);
    }
    
    KRT_FREE(ssa_builder);
}

typedef struct {
    char** names;
    int count;
    int capacity;
} VarNameList;

static void add_var_name(VarNameList* list, const char* name) {
    if (!list || !name) return;
    
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->names[i], name) == 0) {
            return;
        }
    }
    
    if (list->count >= list->capacity) {
        list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
        char** new_names = (char**)KRT_REALLOC(list->names, list->capacity * sizeof(char*));
        if (!new_names) return;
        list->names = new_names;
    }
    
    list->names[list->count++] = KRT_STRDUP(name);
}

char** KrtIrSsaCollectVars(KrtIRMemoryArena* arena __attribute__((unused)), KrtIRFunction* func, int* count) {
    if (!func || !count) return NULL;
    
    VarNameList list = {0};
    
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            KrtIRInst* inst = block->insts[i];
            if (!inst) continue;
            
            if ((inst->opcode == KRT_IR_LOAD || inst->opcode == KRT_IR_STORE) &&
                inst->operand_count > 0) {
                KrtIRValue* val = &inst->operands[0];
                if (val->type == KRT_IR_VALUE_VAR && val->data.name) {
                    add_var_name(&list, val->data.name);
                }
            }
        }
        block = block->next;
    }
    
    *count = list.count;
    return list.names;
}

void KrtIrSsaConstruct(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    KrtIrSsaInsertPhis(ssa_builder, func);
    
    KrtIrSsaRenameVars(ssa_builder, func);
}

void KrtIrSsaInsertPhis(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    int var_count = 0;
    char** vars = KrtIrSsaCollectVars(ssa_builder->arena, func, &var_count);
    
    for (int i = 0; i < var_count; i++) {
        KrtIrSsaInsertPhisForVar(ssa_builder, func, vars[i]);
        KRT_FREE(vars[i]);
    }
    
    KRT_FREE(vars);
}

void KrtIrSsaInsertPhisForVar(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func, const char* var_name) {
    if (!ssa_builder || !func || !var_name) return;
    
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        if (block->pred_count > 1) {
            
        }
        block = block->next;
    }
}

void KrtIrSsaRenameVars(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func) {
    if (!ssa_builder || !func) return;
    
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            KrtIRInst* inst = block->insts[i];
            if (!inst) continue;
            
            if (inst->opcode == KRT_IR_STORE && inst->operand_count > 0) {
                KrtIRValue* val = &inst->operands[0];
                if (val->type == KRT_IR_VALUE_VAR && val->data.name) {
                    
                    KrtIRVarVersion* version = KrtIrVarNewVersion(
                        ssa_builder->var_table,
                        val->data.name,
                        NULL,  
                        block,
                        inst
                    );
                    (void)version;
                }
            }
        }
        block = block->next;
    }
}

void KrtIrSsaComputeDominanceFrontier(KrtIRFunction* func) {
    
    (void)func;
}

void KrtIrSsaComputeDominatorTree(KrtIRFunction* func) {
    
    (void)func;
}

bool KrtIrSsaDominates(KrtIRBasicBlock* dom, KrtIRBasicBlock* block) {
    
    if (!dom || !block) return false;
    return dom == block;
}

bool KrtIrSsaVerify(KrtIRFunction* func) {
    if (!func) return false;
    
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        for (int i = 0; i < block->inst_count; i++) {
            KrtIRInst* inst = block->insts[i];
            if (!inst) continue;
            
        }
        block = block->next;
    }
    
    return true;
}