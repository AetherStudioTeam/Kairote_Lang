#ifndef KRT_VM_OBJECT_H
#define KRT_VM_OBJECT_H

#include "Core/Utils/KrtCommon.h"
#include "Value.h"

typedef enum {
    OBJ_STRING,
} KrtObjType;

struct KrtObject {
    KrtObjType type;
    bool is_marked;
    struct KrtObject* next; 
};

typedef struct {
    struct KrtObject obj;
    int length;
    char* chars;
} KrtString;

static inline bool is_obj_type(KrtValue value, KrtObjType type) {
    return IS_OBJ(value) && ((struct KrtObject*)AS_OBJ(value))->type == type;
}

#define IS_STRING(value) (is_obj_type(value, OBJ_STRING))
#define IS_STRING_LIT(value) ((value).type == VAL_STRING_LITERAL)
#define IS_STRING_VAL(value) (IS_STRING(value) || IS_STRING_LIT(value))

#define AS_STRING(value) ((KrtString*)AS_OBJ(value))
#define AS_STRING_LIT(value) ((value).as.string_literal)
#define AS_CSTRING(value) (IS_STRING_LIT(value) ? AS_STRING_LIT(value) : (IS_STRING(value) ? AS_STRING(value)->chars : ""))
#define AS_STRING_LEN(value) (IS_STRING_LIT(value) ? (int)strlen(AS_STRING_LIT(value)) : (IS_STRING(value) ? AS_STRING(value)->length : 0))

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

KrtString* KrtObjectNewString(void* vm, const char* chars, int length);
KrtString* KrtObjectTakeString(void* vm, char* chars, int length);

#endif 