#ifndef KRT_IR_PROFILE_H
#define KRT_IR_PROFILE_H

#include "../../../Core/Utils/KrtCommon.h"
#include <time.h>

typedef enum {
    KRT_PROFILE_LEXER,        
    KRT_PROFILE_PARSER,       
    KRT_PROFILE_SEMANTIC,     
    KRT_PROFILE_IR_GEN,       
    KRT_PROFILE_IR_OPT,       
    KRT_PROFILE_CODEGEN,      
    KRT_PROFILE_LINKING,      
    KRT_PROFILE_TOTAL,        
    KRT_PROFILE_PHASE_COUNT   
} KrtProfilePhase;

typedef struct {
    double times[KRT_PROFILE_PHASE_COUNT];  
    int counts[KRT_PROFILE_PHASE_COUNT];    
    clock_t start_times[KRT_PROFILE_PHASE_COUNT];  
    int active[KRT_PROFILE_PHASE_COUNT];    
} KrtIRProfiler;

typedef struct {
    size_t total_allocated;    
    size_t total_freed;        
    size_t current_used;       
    size_t peak_used;          
    size_t allocation_count;   
    size_t free_count;         
} KrtIRMemoryStats;

typedef struct {
    
    int inst_count;            
    int block_count;           
    int function_count;        
    int var_count;             
    
    int opt_passes;            
    int opt_applied;           
    
    int arena_allocs;          
    int malloc_allocs;         
    size_t arena_bytes;        
} KrtIRPerformanceCounters;

extern KrtIRProfiler g_ir_profiler;
extern KrtIRMemoryStats g_ir_memory_stats;
extern KrtIRPerformanceCounters g_ir_counters;

void KrtIrProfilerInit(void);
void KrtIrProfilerReset(void);
void KrtIrProfilerPrint(void);

void KrtIrProfileBegin(KrtProfilePhase phase);
void KrtIrProfileEnd(KrtProfilePhase phase);

void KrtIrMemoryTrackAlloc(size_t size);
void KrtIrMemoryTrackFree(size_t size);
void KrtIrMemoryPrint(void);

void KrtIrCountersReset(void);
void KrtIrCountersPrint(void);

#ifdef ENABLE_IR_PROFILING
    #define IR_PROFILE_BEGIN(phase) KrtIrProfileBegin(phase)
    #define IR_PROFILE_END(phase) KrtIrProfileEnd(phase)
    #define IR_TRACK_ALLOC(size) KrtIrMemoryTrackAlloc(size)
    #define IR_TRACK_FREE(size) KrtIrMemoryTrackFree(size)
#else
    #define IR_PROFILE_BEGIN(phase) ((void)0)
    #define IR_PROFILE_END(phase) ((void)0)
    #define IR_TRACK_ALLOC(size) ((void)0)
    #define IR_TRACK_FREE(size) ((void)0)
#endif

#define IR_PROFILE_SCOPE(phase) \
    for (struct { int i; } _ir_profile_scope = (KrtIrProfileBegin(phase), (struct { int i; }){0}); \
         _ir_profile_scope.i == 0; \
         KrtIrProfileEnd(phase), _ir_profile_scope.i = 1)

#endif 