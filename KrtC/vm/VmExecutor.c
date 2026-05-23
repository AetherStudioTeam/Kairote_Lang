#include "VmExecutor.h"
#include "Core/Utils/KrtCommon.h"
#include <stdio.h>
#include <stdlib.h>

VMExecutor* vm_executor_create(const char* bytecode_file_path, int verbose) {
    if (!bytecode_file_path) {
        return NULL;
    }
    
    VMExecutor* executor = (VMExecutor*)KRT_MALLOC(sizeof(VMExecutor));
    if (!executor) {
        return NULL;
    }
    
    KrtVmInit(&executor->vm);
    
    size_t path_len = strlen(bytecode_file_path);
    executor->bytecode_file_path = (char*)KRT_MALLOC(path_len + 1);
    if (!executor->bytecode_file_path) {
        free(executor);
        return NULL;
    }
    strcpy(executor->bytecode_file_path, bytecode_file_path);
    
    executor->verbose = verbose;
    
    return executor;
}

void vm_executor_destroy(VMExecutor* executor) {
    if (!executor) {
        return;
    }
    
    KrtVmFree(&executor->vm);
    
    if (executor->bytecode_file_path) {
        free(executor->bytecode_file_path);
    }
    
    free(executor);
}

KrtChunk* vm_executor_load_bytecode(VMExecutor* executor) {
    if (!executor || !executor->bytecode_file_path) {
        return NULL;
    }
    
    FILE* file = fopen(executor->bytecode_file_path, "rb");
    if (!file) {
        if (executor->verbose) {
            fprintf(stderr, "无法打开字节码文件: %s\n", executor->bytecode_file_path);
        }
        return NULL;
    }
    
    uint32_t magic;
    uint16_t version;
    if (fread(&magic, sizeof(uint32_t), 1, file) != 1 || 
        fread(&version, sizeof(uint16_t), 1, file) != 1) {
        fclose(file);
        if (executor->verbose) {
            fprintf(stderr, "无法读取字节码文件头: %s\n", executor->bytecode_file_path);
        }
        return NULL;
    }
    
    if (magic != 0x45534243) { 
        fclose(file);
        if (executor->verbose) {
            fprintf(stderr, "无效的字节码文件格式: %s\n", executor->bytecode_file_path);
        }
        return NULL;
    }
    
    if (version != 1) {
        fclose(file);
        if (executor->verbose) {
            fprintf(stderr, "不支持的字节码文件版本: %s (版本 %d)\n", executor->bytecode_file_path, version);
        }
        return NULL;
    }
    
    KrtChunk* chunk = (KrtChunk*)KRT_MALLOC(sizeof(KrtChunk));
    if (!chunk) {
        fclose(file);
        return NULL;
    }
    
    KrtChunkInit(chunk);
    
    uint32_t code_count;
    if (fread(&code_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        KrtChunkFree(chunk);
        free(chunk);
        return NULL;
    }
    
    if (code_count > 0) {
        
        chunk->code = (uint8_t*)KRT_MALLOC(code_count);
        chunk->lines = (int*)KRT_MALLOC(code_count * sizeof(int));
        
        if (!chunk->code || !chunk->lines) {
            fclose(file);
            KrtChunkFree(chunk);
            free(chunk);
            return NULL;
        }
        
        if (fread(chunk->code, sizeof(uint8_t), code_count, file) != code_count ||
            fread(chunk->lines, sizeof(int), code_count, file) != code_count) {
            fclose(file);
            KrtChunkFree(chunk);
            free(chunk);
            return NULL;
        }
        
        chunk->count = code_count;
        chunk->capacity = code_count;
    }
    
    uint32_t constant_count;
    if (fread(&constant_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        KrtChunkFree(chunk);
        free(chunk);
        return NULL;
    }
    
    if (constant_count > 0) {
        
        chunk->constants.values = (KrtValue*)KRT_MALLOC(constant_count * sizeof(KrtValue));
        if (!chunk->constants.values) {
            fclose(file);
            KrtChunkFree(chunk);
            free(chunk);
            return NULL;
        }
        
        for (int i = 0; i < constant_count; i++) {
            KrtValue value;
            if (fread(&value.type, sizeof(KrtValueType), 1, file) != 1) {
                fclose(file);
                KrtChunkFree(chunk);
                free(chunk);
                return NULL;
            }
            
            switch (value.type) {
                case VAL_BOOL:
                    if (fread(&value.as.boolean, sizeof(bool), 1, file) != 1) {
                        fclose(file);
                        KrtChunkFree(chunk);
                        free(chunk);
                        return NULL;
                    }
                    break;
                case VAL_NUMBER:
                    if (fread(&value.as.number, sizeof(double), 1, file) != 1) {
                        fclose(file);
                        KrtChunkFree(chunk);
                        free(chunk);
                        return NULL;
                    }
                    break;
                case VAL_STRING_LITERAL: {
                    uint16_t length;
                    if (fread(&length, sizeof(uint16_t), 1, file) != 1) {
                        fclose(file);
                        KrtChunkFree(chunk);
                        free(chunk);
                        return NULL;
                    }
                    
                    char* string = (char*)KRT_MALLOC(length + 1);
                    if (!string) {
                        fclose(file);
                        KrtChunkFree(chunk);
                        free(chunk);
                        return NULL;
                    }
                    
                    if (fread(string, sizeof(char), length, file) != length) {
                        free(string);
                        fclose(file);
                        KrtChunkFree(chunk);
                        free(chunk);
                        return NULL;
                    }
                    
                    string[length] = '\0';
                    value.as.string_literal = string;
                    break;
                }
                case VAL_NULL:
                case VAL_OBJ:
                    
                    break;
            }
            
            chunk->constants.values[i] = value;
        }
        
        chunk->constants.count = constant_count;
        chunk->constants.capacity = constant_count;
    }
    
    fclose(file);
    return chunk;
}
void vm_executor_free_chunk(VMExecutor* executor, KrtChunk* chunk) {
    if (!executor || !chunk) {
        return;
    }
    
    KrtChunkFree(chunk);
    free(chunk);
}

KrtInterpretResult vm_executor_execute(VMExecutor* executor) {
    if (!executor) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    KrtChunk* chunk = vm_executor_load_bytecode(executor);
    if (!chunk) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    KrtInterpretResult result = KrtVmInterpret(&executor->vm, chunk);
    
    vm_executor_free_chunk(executor, chunk);
    
    return result;
}