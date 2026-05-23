#ifndef KRT_IR_ASSERT_H
#define KRT_IR_ASSERT_H

#include "Ir.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KRT_IR_ASSERT_ENABLED
    #ifdef DEBUG
        #define KRT_IR_ASSERT_ENABLED 1
    #else
        #define KRT_IR_ASSERT_ENABLED 0
    #endif
#endif

#ifndef KRT_IR_BOUNDS_CHECK_ENABLED
    #ifdef DEBUG
        #define KRT_IR_BOUNDS_CHECK_ENABLED 1
    #else
        #define KRT_IR_BOUNDS_CHECK_ENABLED 0
    #endif
#endif

#if KRT_IR_ASSERT_ENABLED
    #define KRT_IR_ASSERT(cond) do { \
        if (!(cond)) { \
            KrtError("IR_ASSERT failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
            assert(cond); \
        } \
    } while(0)
    
    #define KRT_IR_ASSERT_MSG(cond, msg) do { \
        if (!(cond)) { \
            KrtError("IR_ASSERT failed: %s - %s at %s:%d", #cond, msg, __FILE__, __LINE__); \
            assert(cond); \
        } \
    } while(0)
    
    #define KRT_IR_ASSERT_NOT_NULL(ptr) KRT_IR_ASSERT_MSG((ptr) != NULL, #ptr " must not be NULL")
    
    #define KRT_IR_ASSERT_VALID_BUILDER(builder) do { \
        KRT_IR_ASSERT_NOT_NULL(builder); \
        KRT_IR_ASSERT_NOT_NULL((builder)->arena); \
        KRT_IR_ASSERT_NOT_NULL((builder)->module); \
    } while(0)
    
    #define KRT_IR_ASSERT_VALID_FUNCTION(func) do { \
        KRT_IR_ASSERT_NOT_NULL(func); \
        KRT_IR_ASSERT_NOT_NULL((func)->name); \
        KRT_IR_ASSERT((func)->param_count >= 0); \
    } while(0)
    
    #define KRT_IR_ASSERT_VALID_BLOCK(block) do { \
        KRT_IR_ASSERT_NOT_NULL(block); \
        KRT_IR_ASSERT_NOT_NULL((block)->label); \
    } while(0)
    
    #define KRT_IR_ASSERT_VALID_INST(inst) do { \
        KRT_IR_ASSERT_NOT_NULL(inst); \
        KRT_IR_ASSERT((inst)->opcode >= KRT_IR_LOAD && (inst)->opcode <= KRT_IR_NOP); \
        KRT_IR_ASSERT((inst)->operand_count >= 0); \
        KRT_IR_ASSERT((inst)->operand_count <= (inst)->operand_capacity); \
    } while(0)
    
    #define KRT_IR_ASSERT_VALID_VALUE(val) do { \
        KRT_IR_ASSERT((val).type >= KRT_IR_VALUE_VOID && (val).type <= KRT_IR_VALUE_FUNCTION); \
    } while(0)
    
    #define KRT_IR_ASSERT_VALID_ARENA(arena) do { \
        KRT_IR_ASSERT_NOT_NULL(arena); \
        KRT_IR_ASSERT_NOT_NULL((arena)->current_pool); \
        KRT_IR_ASSERT((arena)->pool_size > 0); \
    } while(0)
#else
    #define KRT_IR_ASSERT(cond) ((void)0)
    #define KRT_IR_ASSERT_MSG(cond, msg) ((void)0)
    #define KRT_IR_ASSERT_NOT_NULL(ptr) ((void)0)
    #define KRT_IR_ASSERT_VALID_BUILDER(builder) ((void)0)
    #define KRT_IR_ASSERT_VALID_FUNCTION(func) ((void)0)
    #define KRT_IR_ASSERT_VALID_BLOCK(block) ((void)0)
    #define KRT_IR_ASSERT_VALID_INST(inst) ((void)0)
    #define KRT_IR_ASSERT_VALID_VALUE(val) ((void)0)
    #define KRT_IR_ASSERT_VALID_ARENA(arena) ((void)0)
#endif

#if KRT_IR_BOUNDS_CHECK_ENABLED
    #define KRT_IR_BOUNDS_CHECK(index, count) do { \
        if ((index) < 0 || (index) >= (count)) { \
            KrtError("IR_BOUNDS_CHECK failed: index=%d, count=%d at %s:%d", \
                     (int)(index), (int)(count), __FILE__, __LINE__); \
            assert((index) >= 0 && (index) < (count)); \
        } \
    } while(0)
    
    #define KRT_IR_BOUNDS_CHECK_ARRAY(ptr, index, count) do { \
        KRT_IR_ASSERT_NOT_NULL(ptr); \
        KRT_IR_BOUNDS_CHECK(index, count); \
    } while(0)
    
    #define KRT_IR_CAPACITY_CHECK(current, capacity) do { \
        if ((current) >= (capacity)) { \
            KrtError("IR_CAPACITY_CHECK failed: current=%d, capacity=%d at %s:%d", \
                     (int)(current), (int)(capacity), __FILE__, __LINE__); \
            assert((current) < (capacity)); \
        } \
    } while(0)
#else
    #define KRT_IR_BOUNDS_CHECK(index, count) ((void)0)
    #define KRT_IR_BOUNDS_CHECK_ARRAY(ptr, index, count) ((void)0)
    #define KRT_IR_CAPACITY_CHECK(current, capacity) ((void)0)
#endif

#define KRT_IR_ALLOC_FAIL_ABORT 0
#define KRT_IR_ALLOC_FAIL_RETURN_NULL 1
#define KRT_IR_ALLOC_FAIL_LOG_AND_RETURN 2

#ifndef KRT_IR_ALLOC_FAIL_POLICY
    #define KRT_IR_ALLOC_FAIL_POLICY KRT_IR_ALLOC_FAIL_LOG_AND_RETURN
#endif

#if KRT_IR_ALLOC_FAIL_POLICY == KRT_IR_ALLOC_FAIL_ABORT
    #define KRT_IR_HANDLE_ALLOC_FAIL(msg) do { \
        KrtError("IR_ALLOC_FAIL: %s at %s:%d", msg, __FILE__, __LINE__); \
        abort(); \
    } while(0)
#elif KRT_IR_ALLOC_FAIL_POLICY == KRT_IR_ALLOC_FAIL_RETURN_NULL
    #define KRT_IR_HANDLE_ALLOC_FAIL(msg) do { \
        return NULL; \
    } while(0)
#else 
    #define KRT_IR_HANDLE_ALLOC_FAIL(msg) do { \
        KrtError("IR_ALLOC_FAIL: %s at %s:%d", msg, __FILE__, __LINE__); \
        return NULL; \
    } while(0)
#endif

#define KRT_IR_ALLOC_CHECKED(ptr, type, arena) do { \
    (ptr) = (type*)KrtIrArenaAlloc((arena), sizeof(type)); \
    if (KRT_IR_UNLIKELY(!(ptr))) { \
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate " #type); \
    } \
} while(0)

#define KRT_IR_ALLOC_ARRAY_CHECKED(ptr, type, count, arena) do { \
    (ptr) = (type*)KrtIrArenaAlloc((arena), sizeof(type) * (count)); \
    if (KRT_IR_UNLIKELY(!(ptr))) { \
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate array of " #type); \
    } \
} while(0)

#define KRT_IR_STRDUP_CHECKED(ptr, str, arena) do { \
    (ptr) = KrtIrArenaStrdup((arena), (str)); \
    if (KRT_IR_UNLIKELY(!(ptr))) { \
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to duplicate string"); \
    } \
} while(0)

bool KrtIrBuilderIsValid(KrtIRBuilder* builder);

bool KrtIrFunctionIsValid(KrtIRFunction* func);

bool KrtIrBlockIsValid(KrtIRBasicBlock* block);

bool KrtIrInstIsValid(KrtIRInst* inst);

bool KrtIrValueIsValid(KrtIRValue* value);

bool KrtIrArenaIsValid(KrtIRMemoryArena* arena);

bool KrtIrOperandIndexIsValid(KrtIRInst* inst, int index);

bool KrtIrParamIndexIsValid(KrtIRFunction* func, int index);

#ifdef __cplusplus
}
#endif

#endif 