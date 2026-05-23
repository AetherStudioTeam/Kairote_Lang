#ifndef KRT_KRO_CODEGEN_H
#define KRT_KRO_CODEGEN_H

#include "../../Middle/Ir/Ir.h"
#include "../../../Tools/KroWriter.h"
#include <stdio.h>

typedef struct {
    KROWriter* writer;
    FILE* output_file;
    char output_filename[256];
    int temp_var_count;
    int label_count;
    int32_t* string_const_sym_indices;
} KROCodegenContext;

void KrtKrtGenerate(FILE* output_file, const char* output_filename, KrtIRModule* module);

static void KroGenerateFunction(KROCodegenContext* ctx, KrtIRFunction* func, KrtIRModule* module);

static void KroGenerateBlock(KROCodegenContext* ctx, KrtIRBasicBlock* block);

static void KroGenerateInstruction(KROCodegenContext* ctx, KrtIRInst* inst);

#endif 