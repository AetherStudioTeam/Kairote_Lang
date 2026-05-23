#include <string.h>
#include "Object.h"
#include "Vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocate_object(vm, sizeof(type), objectType)

static struct KrtObject* allocate_object(KrtVM* vm, size_t size, KrtObjType type) {
    struct KrtObject* object = (struct KrtObject*)KrtVmReallocate(vm, NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    
    object->next = vm->objects;
    vm->objects = object;
    
    return object;
}

KrtString* KrtObjectNewString(void* vm, const char* chars, int length) {
    char* heap_chars = (char*)KrtVmReallocate((KrtVM*)vm, NULL, 0, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return KrtObjectTakeString(vm, heap_chars, length);
}

KrtString* KrtObjectTakeString(void* vm, char* chars, int length) {
    KrtString* string = ALLOCATE_OBJ((KrtVM*)vm, KrtString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}