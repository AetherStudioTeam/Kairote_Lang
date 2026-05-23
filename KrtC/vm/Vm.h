#ifndef KRT_VM_H
#define KRT_VM_H

#include "Bytecode.h"
#include "Value.h"

#define STACK_MAX 256
#define FRAMES_MAX 64

typedef struct {
    uint8_t* ip;          
    KrtValue* slots;       
} KrtCallFrame;

typedef struct {
    KrtChunk* chunk;
    uint8_t* ip;          
    
    KrtValue stack[STACK_MAX];
    KrtValue* stack_top;   

    KrtCallFrame frames[FRAMES_MAX];
    int frame_count;

    size_t bytes_allocated;
    size_t next_gc;
    struct KrtObject* objects; 
} KrtVM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} KrtInterpretResult;

void KrtVmInit(KrtVM* vm);
void KrtVmFree(KrtVM* vm);
KrtInterpretResult KrtVmInterpret(KrtVM* vm, KrtChunk* chunk);

void KrtVmPush(KrtVM* vm, KrtValue value);
KrtValue KrtVmPop(KrtVM* vm);

void* KrtVmReallocate(KrtVM* vm, void* pointer, size_t old_size, size_t new_size);
void KrtVmCollectGarbage(KrtVM* vm);

#endif 