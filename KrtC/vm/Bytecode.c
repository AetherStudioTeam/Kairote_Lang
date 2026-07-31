#include "Bytecode.h"
#include "Core/Utils/KrtCommon.h"
#include "../src/Runtime/Runtime.h"
#include <stdlib.h>

void KrtChunkInit(KrtChunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    KrtValueArrayInit(&chunk->constants);
}

void KrtChunkFree(KrtChunk* chunk) {
    KrtFree(chunk->code);
    KrtFree(chunk->lines);
    
    for (int i = 0; i < chunk->constants.count; i++) {
        if (IS_STRING_LIT(chunk->constants.values[i])) {
            KrtFree((void*)AS_STRING_LIT(chunk->constants.values[i]));
        }
    }
    
    KrtValueArrayFree(&chunk->constants);
    KrtChunkInit(chunk);
}

void KrtChunkWrite(KrtChunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = (uint8_t*)KRT_REALLOC(chunk->code, sizeof(uint8_t) * chunk->capacity);
        chunk->lines = (int*)KRT_REALLOC(chunk->lines, sizeof(int) * chunk->capacity);
    }
    
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int KrtChunkAddConstant(KrtChunk* chunk, KrtValue value) {
    KrtValueArrayWrite(&chunk->constants, value);
    return chunk->constants.count - 1;
}

#include <string.h>

int KrtChunkAddStringConstant(KrtChunk* chunk, const char* s) {
    
    size_t len = strlen(s);
    char* copy = (char*)KRT_MALLOC(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    KrtValue value = STRING_VAL(copy);
    return KrtChunkAddConstant(chunk, value);
}