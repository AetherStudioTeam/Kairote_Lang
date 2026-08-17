#ifndef KRT_VM_DEBUG_H
#define KRT_VM_DEBUG_H

#include "Bytecode.h"

void KrtDisassembleChunk(KrtChunk* chunk, const char* name);
int KrtDisassembleInstruction(KrtChunk* chunk, int offset);

#endif 