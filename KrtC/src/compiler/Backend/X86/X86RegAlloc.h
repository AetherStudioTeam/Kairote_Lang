
#ifndef KRT_X86_REGALLOC_H
#define KRT_X86_REGALLOC_H

#include "X86Codegen.h"

#define X86_MAX_INSTRUCTIONS 10000
#define X86_LIVESET_BITS 256
#define X86_LIVESET_BYTES (X86_LIVESET_BITS / 8)
#define X86_MAX_CONFLICT_NODES 256
#define X86_NUM_ALLOCABLE_REGS 2

typedef struct {
    unsigned char bits[X86_LIVESET_BYTES];
} LiveSet;

typedef struct {
    LiveSet use;
    LiveSet def;
    LiveSet live_in;
    LiveSet live_out;
} BlockLiveInfo;

typedef struct {
    BlockLiveInfo* block_info;
    int block_count;
    LiveSet* inst_live;
    int inst_count;
} LivenessAnalysis;

LivenessAnalysis* liveness_analysis_create(KrtIRFunction* func);
void liveness_analysis_destroy(LivenessAnalysis* analysis);
void liveness_analysis_run(LivenessAnalysis* analysis, KrtIRFunction* func);
int liveness_is_live(LiveSet* set, int temp_idx);
void liveness_add(LiveSet* set, int temp_idx);
void liveness_remove(LiveSet* set, int temp_idx);
void liveness_union(LiveSet* result, LiveSet* a, LiveSet* b);

struct ConflictNodeStruct;
typedef struct ConflictNodeStruct ConflictNode;

struct ConflictNodeStruct {
    int temp_idx;
    int degree;
    int color;
    int is_spilled;
    int stack_offset;
    ConflictNode** neighbors;
    int neighbor_count;
    int neighbor_capacity;
};

typedef struct {
    ConflictNode* nodes[X86_MAX_CONFLICT_NODES];
    int node_count;
} ConflictGraph;

ConflictGraph* conflict_graph_create(void);
void conflict_graph_destroy(ConflictGraph* graph);
void conflict_graph_build(ConflictGraph* graph, LivenessAnalysis* liveness, KrtIRFunction* func);
void conflict_graph_add_edge(ConflictGraph* graph, int temp_a, int temp_b);
int conflict_graph_are_conflicting(ConflictGraph* graph, int temp_a, int temp_b);

static const char* g_allocable_regs[X86_NUM_ALLOCABLE_REGS] = {
    "r10", "r11"
};

typedef struct {
    int temp_to_reg[X86_MAX_TEMP_LOCATIONS];
    int temp_to_stack[X86_MAX_TEMP_LOCATIONS];
    int num_spilled;
    int stack_space_needed;
} RegAllocResult;

RegAllocResult* regalloc_allocate(ConflictGraph* graph);
void regalloc_result_destroy(RegAllocResult* result);
const char* regalloc_get_reg_name(RegAllocResult* result, int temp_idx);
int regalloc_get_stack_offset(RegAllocResult* result, int temp_idx);

RegAllocResult* x86_allocate_registers(KrtIRFunction* func);

#endif