#ifndef KRT_X86_BACKEND_H
#define KRT_X86_BACKEND_H

#include <stdio.h>
#include "../../Middle/Ir/Ir.h"

#define X86_MAX_TEMP_LOCATIONS 256
#define X86_MAX_REGISTERS 14
#define X86_MAX_VAR_OFFSETS 100
#define X86_MAX_VAR_NAME_LEN 64

typedef enum {
    TEMP_LOC_NONE,
    TEMP_LOC_REGISTER,
    TEMP_LOC_STACK
} TempLocationType;

typedef struct {
    TempLocationType type;
    union {
        const char* reg;
        int offset;
    };
} TempLocation;

typedef struct {
    const char* name;
    int is_free;
    const char* content;
} RegisterState;

typedef struct {
    KrtIRFunction* current_func;
    KrtIRModule* current_module;
    TempLocation temp_locations[X86_MAX_TEMP_LOCATIONS];
    RegisterState registers[X86_MAX_REGISTERS];
    int stack_size;
    int temp_stack_base;
    struct {
        char name[X86_MAX_VAR_NAME_LEN];
        int offset;
    } var_offsets[X86_MAX_VAR_OFFSETS];
    int var_count;
    int next_var_offset;
    int temp_usage[X86_MAX_TEMP_LOCATIONS];
    void* regalloc;
} CodegenContext;

void KrtX86Generate(FILE* output, KrtIRModule* module);

void codegen_context_init(CodegenContext* ctx, KrtIRFunction* func, KrtIRModule* module);
TempLocation* codegen_get_temp_location(CodegenContext* ctx, int temp_idx);
void codegen_set_temp_in_register(CodegenContext* ctx, int temp_idx, const char* reg);
void codegen_set_temp_on_stack(CodegenContext* ctx, int temp_idx, int offset);
const char* codegen_alloc_register(CodegenContext* ctx);
void codegen_free_register(CodegenContext* ctx, const char* reg);
int codegen_get_var_offset(CodegenContext* ctx, const char* var_name);

void codegen_emit_load_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* target_reg);
void codegen_emit_store_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* source_reg);
void codegen_emit_load_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* target_reg);
void codegen_emit_store_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* source_reg);

#endif