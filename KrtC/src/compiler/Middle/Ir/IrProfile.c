#include "IrProfile.h"
#include <stdio.h>
#include <string.h>

KrtIRProfiler g_ir_profiler = {0};
KrtIRMemoryStats g_ir_memory_stats = {0};
KrtIRPerformanceCounters g_ir_counters = {0};

void KrtIrProfilerInit(void) {
    memset(&g_ir_profiler, 0, sizeof(g_ir_profiler));
}

void KrtIrProfilerReset(void) {
    KrtIrProfilerInit();
}

static const char* phase_name(KrtProfilePhase phase) {
    switch (phase) {
        case KRT_PROFILE_LEXER:    return "Lexer     ";
        case KRT_PROFILE_PARSER:   return "Parser    ";
        case KRT_PROFILE_SEMANTIC: return "Semantic  ";
        case KRT_PROFILE_IR_GEN:   return "IR Gen    ";
        case KRT_PROFILE_IR_OPT:   return "IR Opt    ";
        case KRT_PROFILE_CODEGEN:  return "Codegen   ";
        case KRT_PROFILE_LINKING:  return "Linking   ";
        case KRT_PROFILE_TOTAL:    return "TOTAL     ";
        default:                  return "Unknown   ";
    }
}

void KrtIrProfilerPrint(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║            IR Compilation Profile                ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Phase          Time (ms)    Calls   Avg (ms)     ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    
    double total = 0;
    for (int i = 0; i < KRT_PROFILE_PHASE_COUNT; i++) {
        if (g_ir_profiler.times[i] > 0) {
            double avg = g_ir_profiler.counts[i] > 0 
                ? g_ir_profiler.times[i] / g_ir_profiler.counts[i] 
                : 0;
            printf("║ %s  %8.3f    %5d   %8.3f    ║\n",
                   phase_name(i),
                   g_ir_profiler.times[i],
                   g_ir_profiler.counts[i],
                   avg);
            total += g_ir_profiler.times[i];
        }
    }
    
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ %s  %8.3f                        ║\n", 
           phase_name(KRT_PROFILE_TOTAL), total);
    printf("╚══════════════════════════════════════════════════╝\n");
}

void KrtIrProfileBegin(KrtProfilePhase phase) {
    if (phase < 0 || phase >= KRT_PROFILE_PHASE_COUNT) return;
    if (g_ir_profiler.active[phase]) return;  
    
    g_ir_profiler.start_times[phase] = clock();
    g_ir_profiler.active[phase] = 1;
    g_ir_profiler.counts[phase]++;
}

void KrtIrProfileEnd(KrtProfilePhase phase) {
    if (phase < 0 || phase >= KRT_PROFILE_PHASE_COUNT) return;
    if (!g_ir_profiler.active[phase]) return;  
    
    clock_t end = clock();
    double elapsed = ((double)(end - g_ir_profiler.start_times[phase])) 
                     * 1000.0 / CLOCKS_PER_SEC;
    g_ir_profiler.times[phase] += elapsed;
    g_ir_profiler.active[phase] = 0;
}

void KrtIrMemoryTrackAlloc(size_t size) {
    g_ir_memory_stats.total_allocated += size;
    g_ir_memory_stats.current_used += size;
    g_ir_memory_stats.allocation_count++;
    
    if (g_ir_memory_stats.current_used > g_ir_memory_stats.peak_used) {
        g_ir_memory_stats.peak_used = g_ir_memory_stats.current_used;
    }
}

void KrtIrMemoryTrackFree(size_t size) {
    g_ir_memory_stats.total_freed += size;
    g_ir_memory_stats.current_used -= size;
    g_ir_memory_stats.free_count++;
}

void KrtIrMemoryPrint(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║            IR Memory Statistics                  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Total Allocated:    %10zu bytes            ║\n", g_ir_memory_stats.total_allocated);
    printf("║ Total Freed:        %10zu bytes            ║\n", g_ir_memory_stats.total_freed);
    printf("║ Current Used:       %10zu bytes            ║\n", g_ir_memory_stats.current_used);
    printf("║ Peak Used:          %10zu bytes            ║\n", g_ir_memory_stats.peak_used);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Allocation Count:   %10zu                 ║\n", g_ir_memory_stats.allocation_count);
    printf("║ Free Count:         %10zu                 ║\n", g_ir_memory_stats.free_count);
    printf("╚══════════════════════════════════════════════════╝\n");
}

void KrtIrCountersReset(void) {
    memset(&g_ir_counters, 0, sizeof(g_ir_counters));
}

void KrtIrCountersPrint(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         IR Performance Counters                  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Instructions Generated:  %10d             ║\n", g_ir_counters.inst_count);
    printf("║ Basic Blocks:            %10d             ║\n", g_ir_counters.block_count);
    printf("║ Functions:               %10d             ║\n", g_ir_counters.function_count);
    printf("║ Variables:               %10d             ║\n", g_ir_counters.var_count);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Optimization Passes:     %10d             ║\n", g_ir_counters.opt_passes);
    printf("║ Optimizations Applied:   %10d             ║\n", g_ir_counters.opt_applied);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Arena Allocations:       %10d             ║\n", g_ir_counters.arena_allocs);
    printf("║ Malloc Allocations:      %10d             ║\n", g_ir_counters.malloc_allocs);
    printf("║ Arena Bytes:             %10zu bytes        ║\n", g_ir_counters.arena_bytes);
    printf("╚══════════════════════════════════════════════════╝\n");
}