#ifndef KRT_IR_SSA_H
#define KRT_IR_SSA_H

#include "Ir.h"
#include "IrType.h"

typedef struct KrtIRVarVersion {
    char* name;                    
    int version;                   
    KrtIRType* type;                
    KrtIRBasicBlock* block;         
    KrtIRInst* def;                 
    struct KrtIRVarVersion* next;   
} KrtIRVarVersion;

typedef struct {
    KrtIRVarVersion** buckets;      
    int bucket_count;              
    int var_count;                 
} KrtIRVarTable;

typedef struct KrtIRPhi {
    char* var_name;                
    int version;                   
    KrtIRType* type;                
    
    KrtIRBasicBlock** blocks;       
    KrtIRVarVersion** versions;     
    int pred_count;                
    
    KrtIRBasicBlock* parent_block;
    
    struct KrtIRPhi* next;
} KrtIRPhi;

typedef struct {
    KrtIRBuilder* builder;          
    KrtIRVarTable* var_table;       
    KrtIRMemoryArena* arena;        
    
    int* version_counters;         
    int var_capacity;              
} KrtIRSSABuilder;

KrtIRVarTable* KrtIrVarTableCreate(KrtIRMemoryArena* arena, int bucket_count);
void KrtIrVarTableDestroy(KrtIRVarTable* table);

KrtIRVarVersion* KrtIrVarGetVersion(KrtIRVarTable* table, const char* name);
KrtIRVarVersion* KrtIrVarNewVersion(KrtIRVarTable* table, const char* name, KrtIRType* type, 
                                       KrtIRBasicBlock* block, KrtIRInst* def);

KrtIRVarVersion* KrtIrVarFindVersion(KrtIRVarTable* table, const char* name, KrtIRBasicBlock* block);

char* KrtIrVarVersionedName(KrtIRMemoryArena* arena, const char* name, int version);

KrtIRPhi* KrtIrPhiCreate(KrtIRMemoryArena* arena, const char* var_name, KrtIRType* type, 
                           int pred_count, KrtIRBasicBlock* parent);

void KrtIrPhiAddOperand(KrtIRPhi* phi, KrtIRBasicBlock* block, KrtIRVarVersion* version, int index);

KrtIRPhi* KrtIrPhiFind(KrtIRBasicBlock* block, const char* var_name);

void KrtIrBlockAddPhi(KrtIRBasicBlock* block, KrtIRPhi* phi);

KrtIRSSABuilder* KrtIrSsaBuilderCreate(KrtIRBuilder* builder);
void KrtIrSsaBuilderDestroy(KrtIRSSABuilder* ssa_builder);

void KrtIrSsaConstruct(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func);

void KrtIrSsaInsertPhis(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func);

void KrtIrSsaRenameVars(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func);

void KrtIrSsaComputeDominanceFrontier(KrtIRFunction* func);

void KrtIrSsaComputeDominatorTree(KrtIRFunction* func);

bool KrtIrSsaDominates(KrtIRBasicBlock* dom, KrtIRBasicBlock* block);

bool KrtIrSsaVerify(KrtIRFunction* func);

void KrtIrSsaInsertPhisForVar(KrtIRSSABuilder* ssa_builder, KrtIRFunction* func, const char* var_name);

char** KrtIrSsaCollectVars(KrtIRMemoryArena* arena, KrtIRFunction* func, int* count);

#endif 