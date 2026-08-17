#include "Value.h"
#include "Object.h"
#include "../src/Runtime/Runtime.h"
#include <stdio.h>

void KrtValueArrayInit(KrtValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void KrtValueArrayWrite(KrtValueArray* array, KrtValue value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        array->values = (KrtValue*)KRT_REALLOC(array->values, sizeof(KrtValue) * array->capacity);
    }
    
    array->values[array->count] = value;
    array->count++;
}

void KrtValueArrayFree(KrtValueArray* array) {
    KrtFree(array->values);
    KrtValueArrayInit(array);
}

void KrtValuePrint(KrtValue value) {
    switch (value.type) {
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_STRING_LITERAL: printf("%s", AS_STRING_LIT(value)); break;
        case VAL_OBJ: {
            if (IS_STRING_VAL(value)) {
                printf("%s", AS_CSTRING(value));
            } else {
                printf("<obj at %p>", AS_OBJ(value));
            }
            break;
        }
    }
}