
#ifndef X86_CODEOPT_H
#define X86_CODEOPT_H

#include "../../Middle/Ir/Ir.h"

typedef struct X86PeepholeOptimizer X86PeepholeOptimizer;

X86PeepholeOptimizer* x86_peephole_optimizer_create(void);
void x86_peephole_optimizer_destroy(X86PeepholeOptimizer* optimizer);
void x86_optimize_peephole(KrtIRModule* module, int optimization_level);
int x86_peephole_get_optimization_count(X86PeepholeOptimizer* optimizer);

#endif