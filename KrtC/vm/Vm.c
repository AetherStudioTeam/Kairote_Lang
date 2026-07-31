#include "Vm.h"
#include "Object.h"
#include "../src/Runtime/Runtime.h"
#include <stdio.h>
#include <stdarg.h>

void KrtVmInit(KrtVM* vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024; 
}

void* KrtVmReallocate(KrtVM* vm, void* pointer, size_t old_size, size_t new_size) {
    vm->bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
        if (vm->bytes_allocated > vm->next_gc) {
            KrtVmCollectGarbage(vm);
        }
    }

    if (new_size == 0) {
        KrtFree(pointer);
        return NULL;
    }

    void* result = KRT_REALLOC(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

static void free_object(KrtVM* vm, struct KrtObject* object) {
    switch (object->type) {
        case OBJ_STRING: {
            KrtString* string = (KrtString*)object;
            KrtVmReallocate(vm, string->chars, string->length + 1, 0);
            KrtVmReallocate(vm, string, sizeof(KrtString), 0);
            break;
        }
    }
}

void KrtVmFree(KrtVM* vm) {
    struct KrtObject* object = vm->objects;
    while (object != NULL) {
        struct KrtObject* next = object->next;
        free_object(vm, object);
        object = next;
    }
}

static void mark_object(struct KrtObject* object) {
    if (object == NULL || object->is_marked) return;
    object->is_marked = true;
}

static void mark_value(KrtValue value) {
    if (IS_OBJ(value)) mark_object((struct KrtObject*)AS_OBJ(value));
}

static void mark_roots(KrtVM* vm) {
    for (KrtValue* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(*slot);
    }
    
    if (vm->chunk) {
        for (int i = 0; i < vm->chunk->constants.count; i++) {
            mark_value(vm->chunk->constants.values[i]);
        }
    }
}

static void sweep(KrtVM* vm) {
    struct KrtObject* previous = NULL;
    struct KrtObject* object = vm->objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            struct KrtObject* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            free_object(vm, unreached);
        }
    }
}

void KrtVmCollectGarbage(KrtVM* vm) {
    size_t before = vm->bytes_allocated;
    mark_roots(vm);
    sweep(vm);
    
    vm->next_gc = vm->bytes_allocated * 2;
}

void KrtVmPush(KrtVM* vm, KrtValue value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

KrtValue KrtVmPop(KrtVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static void runtime_error(KrtVM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    KrtVmInit(vm);
}

static KrtInterpretResult run(KrtVM* vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_SHORT() \
    (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(vm->stack_top[-1]) || !IS_NUMBER(vm->stack_top[-2])) { \
            runtime_error(vm, "Operands must be numbers. (Types: %d, %d)", \
                          vm->stack_top[-1].type, vm->stack_top[-2].type); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(KrtVmPop(vm)); \
        double a = AS_NUMBER(KrtVmPop(vm)); \
        KrtVmPush(vm, value_type(a op b)); \
    } while (false)

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                KrtValue constant = READ_CONSTANT();
                KrtVmPush(vm, constant);
                break;
            }
            case OP_NULL:  KrtVmPush(vm, NULL_VAL); break;
            case OP_TRUE:  KrtVmPush(vm, BOOL_VAL(true)); break;
            case OP_FALSE: KrtVmPush(vm, BOOL_VAL(false)); break;
            
            case OP_POP: KrtVmPop(vm); break;

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                KrtValue* slots = vm->frames[vm->frame_count - 1].slots;
                KrtVmPush(vm, slots[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                KrtValue* slots = vm->frames[vm->frame_count - 1].slots;
                slots[slot] = vm->stack_top[-1];
                break;
            }

            case OP_EQUAL: {
                KrtValue b = KrtVmPop(vm);
                KrtValue a = KrtVmPop(vm);
                bool eq = false;
                if (a.type == b.type || (IS_STRING_VAL(a) && IS_STRING_VAL(b))) {
                    if (IS_STRING_VAL(a) && IS_STRING_VAL(b)) {
                        const char* s1 = AS_CSTRING(a);
                        const char* s2 = AS_CSTRING(b);
                        int len1 = AS_STRING_LEN(a);
                        int len2 = AS_STRING_LEN(b);
                        eq = (len1 == len2) && (memcmp(s1, s2, len1) == 0);
                    } else {
                        switch (a.type) {
                            case VAL_BOOL:   eq = AS_BOOL(a) == AS_BOOL(b); break;
                            case VAL_NULL:   eq = true; break;
                            case VAL_NUMBER: eq = AS_NUMBER(a) == AS_NUMBER(b); break;
                            default: eq = AS_OBJ(a) == AS_OBJ(b); break;
                        }
                    }
                }
                KrtVmPush(vm, BOOL_VAL(eq));
                break;
            }
            
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:    BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING_VAL(vm->stack_top[-1]) && IS_STRING_VAL(vm->stack_top[-2])) {
                    
                    KrtValue b_val = vm->stack_top[-1];
                    KrtValue a_val = vm->stack_top[-2];
                    
                    const char* b = AS_CSTRING(b_val);
                    const char* a = AS_CSTRING(a_val);
                    int b_len = AS_STRING_LEN(b_val);
                    int a_len = AS_STRING_LEN(a_val);
                    
                    int length = a_len + b_len;
                    char* chars = (char*)KrtVmReallocate(vm, NULL, 0, length + 1);
                    memcpy(chars, a, a_len);
                    memcpy(chars + a_len, b, b_len);
                    chars[length] = '\0';
                    
                    KrtString* result = KrtObjectTakeString(vm, chars, length);
                    
                    KrtVmPop(vm);
                    KrtVmPop(vm);
                    
                    KrtVmPush(vm, OBJ_VAL(result));
                } else if (IS_NUMBER(vm->stack_top[-1]) && IS_NUMBER(vm->stack_top[-2])) {
                    double b = AS_NUMBER(KrtVmPop(vm));
                    double a = AS_NUMBER(KrtVmPop(vm));
                    KrtVmPush(vm, NUMBER_VAL(a + b));
                } else {
                    runtime_error(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB:     BINARY_OP(NUMBER_VAL, -); break;
            case OP_MUL:     BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIV:     BINARY_OP(NUMBER_VAL, /); break;
            
            case OP_NOT:
                vm->stack_top[-1] = BOOL_VAL(IS_NULL(vm->stack_top[-1]) || (IS_BOOL(vm->stack_top[-1]) && !AS_BOOL(vm->stack_top[-1])));
                break;
                
            case OP_NEGATE:
                if (!IS_NUMBER(vm->stack_top[-1])) {
                    runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->stack_top[-1].as.number = -vm->stack_top[-1].as.number;
                break;

            case OP_PRINT: {
                KrtValue val = KrtVmPop(vm);
                KrtValuePrint(val);
                printf("\n");
                break;
            }

            case OP_INT_TO_STRING: {
                KrtValue val = KrtVmPop(vm);
                if (!IS_NUMBER(val)) {
                    runtime_error(vm, "Operand for int_to_string must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                char buffer[32];
                int len = snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(val));
                KrtString* str = KrtObjectNewString(vm, buffer, len);
                KrtVmPush(vm, OBJ_VAL(str));
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (IS_NULL(vm->stack_top[-1]) || (IS_BOOL(vm->stack_top[-1]) && !AS_BOOL(vm->stack_top[-1]))) {
                    vm->ip += offset;
                }
                KrtVmPop(vm); 
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm->ip -= offset;
                break;
            }

            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                int16_t offset = (int16_t)READ_SHORT();
                
            if (vm->frame_count >= FRAMES_MAX) {
                runtime_error(vm, "Stack overflow (too many call frames).");
                return INTERPRET_RUNTIME_ERROR;
            }
            
            KrtCallFrame* frame = &vm->frames[vm->frame_count++];
            frame->ip = vm->ip;
            frame->slots = vm->stack_top - arg_count;
            
            vm->ip += offset;
            break;
            }

            case OP_RETURN: {
                KrtValue result = KrtVmPop(vm);
                vm->frame_count--;
                
                vm->stack_top = vm->frames[vm->frame_count].slots;
                vm->ip = vm->frames[vm->frame_count].ip;
                
                KrtVmPush(vm, result);
                
                if (vm->frame_count == 0) {
                    
                }
                break;
            }

            case OP_STK_ADJ: {
                uint8_t count = READ_BYTE();
                for (int i = 0; i < count; i++) {
                    KrtVmPush(vm, NULL_VAL);
                }
                break;
            }
            
            case OP_HALT:
                return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

KrtInterpretResult KrtVmInterpret(KrtVM* vm, KrtChunk* chunk) {
    vm->chunk = chunk;
    
    for (int i = 0; i < chunk->constants.count; i++) {
        if (IS_STRING_LIT(chunk->constants.values[i])) {
            char* s = (char*)AS_STRING_LIT(chunk->constants.values[i]);
            int length = (int)strlen(s);
            
            KrtString* KrtStr = KrtObjectTakeString(vm, s, length);
            chunk->constants.values[i] = OBJ_VAL(KrtStr);
        }
    }
    
    vm->ip = vm->chunk->code;
    return run(vm);
}