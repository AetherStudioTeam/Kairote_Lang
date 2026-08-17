#ifndef KRT_IR_TYPE_H
#define KRT_IR_TYPE_H

#include "Ir.h"

typedef enum {
    KRT_IR_TYPE_VOID,      
    KRT_IR_TYPE_INT8,      
    KRT_IR_TYPE_INT16,     
    KRT_IR_TYPE_INT32,     
    KRT_IR_TYPE_INT64,     
    KRT_IR_TYPE_UINT8,     
    KRT_IR_TYPE_UINT16,    
    KRT_IR_TYPE_UINT32,    
    KRT_IR_TYPE_UINT64,    
    KRT_IR_TYPE_FLOAT32,   
    KRT_IR_TYPE_FLOAT64,   
    KRT_IR_TYPE_BOOL,      
    KRT_IR_TYPE_CHAR,      
    KRT_IR_TYPE_STRING,    
    KRT_IR_TYPE_POINTER,   
    KRT_IR_TYPE_ARRAY,     
    KRT_IR_TYPE_FUNCTION,  
    KRT_IR_TYPE_STRUCT,    
    KRT_IR_TYPE_CLASS,     
    KRT_IR_TYPE_ANY,       
    KRT_IR_TYPE_UNKNOWN,   
} KrtIRTypeKind;

typedef enum {
    KRT_IR_TYPE_MOD_NONE = 0,
    KRT_IR_TYPE_MOD_CONST = 1 << 0,      
    KRT_IR_TYPE_MOD_VOLATILE = 1 << 1,   
    KRT_IR_TYPE_MOD_REFERENCE = 1 << 2,  
} KrtIRTypeModifier;

struct KrtIRType;
typedef struct KrtIRType KrtIRType;

struct KrtIRType {
    KrtIRTypeKind kind;           
    int modifiers;               
    int size;                    
    int align;                   
    
    union {
        
        struct {
            KrtIRType* pointee;   
        } pointer;
        
        struct {
            KrtIRType* element;   
            int size;            
        } array;
        
        struct {
            KrtIRType** params;   
            int param_count;     
            KrtIRType* ret;       
        } function;
        
        struct {
            char* name;          
            KrtIRType** fields;   
            char** field_names;  
            int field_count;     
        } compound;
    } data;
    
    KrtIRType* next;
};

typedef struct {
    KrtIRType* types;             
    int count;                   
} KrtIRTypePool;

void KrtIrTypePoolInit(KrtIRTypePool* pool);
void KrtIrTypePoolDestroy(KrtIRTypePool* pool);

KrtIRType* KrtIrTypeVoid(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeInt8(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeInt16(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeInt32(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeInt64(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeUint8(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeUint16(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeUint32(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeUint64(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeFloat32(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeFloat64(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeBool(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeChar(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeString(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeAny(KrtIRTypePool* pool);
KrtIRType* KrtIrTypeUnknown(KrtIRTypePool* pool);

KrtIRType* KrtIrTypePointer(KrtIRTypePool* pool, KrtIRType* pointee);
KrtIRType* KrtIrTypeArray(KrtIRTypePool* pool, KrtIRType* element, int size);
KrtIRType* KrtIrTypeFunction(KrtIRTypePool* pool, KrtIRType** params, int param_count, KrtIRType* ret);

KrtIRType* KrtIrTypeStruct(KrtIRTypePool* pool, const char* name);

KrtIRType* KrtIrTypeFromToken(KrtIRTypePool* pool, KrtTokenType token_type);

int KrtIrTypeSize(KrtIRType* type);

int KrtIrTypeAlign(KrtIRType* type);

const char* KrtIrTypeToString(KrtIRType* type);

bool KrtIrTypeEqual(KrtIRType* a, KrtIRType* b);

bool KrtIrTypeCompatible(KrtIRType* src, KrtIRType* dst);

bool KrtIrTypeIsInteger(KrtIRType* type);

bool KrtIrTypeIsUnsigned(KrtIRType* type);

bool KrtIrTypeIsFloat(KrtIRType* type);

bool KrtIrTypeIsNumeric(KrtIRType* type);

bool KrtIrTypeIsPointer(KrtIRType* type);

KrtIRType* KrtIrTypePointee(KrtIRType* type);

KrtIRType* KrtIrTypeBinaryResult(KrtIRTypePool* pool, KrtIRType* lhs, KrtIRType* rhs, KrtIROpcode op);

KrtIRType* KrtIrTypeCompareResult(KrtIRTypePool* pool);

KrtIRType* KrtIrTypePromote(KrtIRTypePool* pool, KrtIRType* type);

KrtIRType* KrtIrTypeCommon(KrtIRTypePool* pool, KrtIRType* a, KrtIRType* b);

bool KrtIrTypeCanAssign(KrtIRType* src, KrtIRType* dst);

bool KrtIrTypeCanCast(KrtIRType* src, KrtIRType* dst);

bool KrtIrTypeSupportsOp(KrtIRType* type, KrtIROpcode op);

KrtIRValue KrtIrTypeDefaultValue(KrtIRType* type);

#endif 