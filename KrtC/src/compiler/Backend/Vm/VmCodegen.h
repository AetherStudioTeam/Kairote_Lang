#ifndef KRT_VM_CODEGEN_H
#define KRT_VM_CODEGEN_H

#include "../../Middle/Ir/Ir.h"
#include "../../../../Shared/Bytecode.h"
#include "../../../../Shared/BytecodeGenerator.h"

void KrtVmCodegenGenerate(KrtIRModule* ir_module, KrtChunk* chunk);

#endif