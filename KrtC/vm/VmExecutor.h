#ifndef KRT_VM_EXECUTOR_H
#define KRT_VM_EXECUTOR_H

#include "Vm.h"
#include "Bytecode.h"

typedef struct {
    KrtVM vm;
    char* bytecode_file_path;
    int verbose;
} VMExecutor;

VMExecutor* vm_executor_create(const char* bytecode_file_path, int verbose);

void vm_executor_destroy(VMExecutor* executor);

KrtInterpretResult vm_executor_execute(VMExecutor* executor);

KrtChunk* vm_executor_load_bytecode(VMExecutor* executor);

void vm_executor_free_chunk(VMExecutor* executor, KrtChunk* chunk);

#endif 