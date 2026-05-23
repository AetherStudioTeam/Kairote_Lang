#ifndef KRT_IR_H
#define KRT_IR_H
#include "../../../Core/Utils/KrtCommon.h"
#include "../../Frontend/FrontendTemp/FrontendTemp/parser/Ast.h"
#include "IrMemory.h"
#include "IrParamTable.h"

#ifndef KRT_IR_INLINE
    #ifdef _MSC_VER
        #define KRT_IR_INLINE __forceinline
    #else
        #define KRT_IR_INLINE static inline __attribute__((always_inline))
    #endif
#endif

#ifndef KRT_IR_LIKELY
    #ifdef __GNUC__
        #define KRT_IR_LIKELY(x) __builtin_expect(!!(x), 1)
        #define KRT_IR_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define KRT_IR_LIKELY(x) (x)
        #define KRT_IR_UNLIKELY(x) (x)
    #endif
#endif
typedef struct KrtIRModule KrtIRModule;
typedef struct KrtIRFunction KrtIRFunction;
typedef struct KrtIRBasicBlock KrtIRBasicBlock;
typedef struct KrtIRInst KrtIRInst;
typedef struct KrtIRBuilder KrtIRBuilder;

typedef enum {
    KRT_IR_VALUE_VOID,
    KRT_IR_VALUE_IMM,
    KRT_IR_VALUE_VAR,
    KRT_IR_VALUE_TEMP,
    KRT_IR_VALUE_ARG,
    KRT_IR_VALUE_STRING_CONST,
    KRT_IR_VALUE_FUNCTION,
} KrtIRValueType;
#define KRT_IR_TYPE_I64 KRT_IR_VALUE_IMM

typedef struct {
    KrtIRValueType type;
    union {
        double imm;
        char* name;
        int index;
        int string_const_id;
        char* function_name;
    } data;
} KrtIRValue;

typedef enum {
    KRT_IR_LOAD,
    KRT_IR_STORE,
    KRT_IR_ALLOC,
    KRT_IR_IMM,    
    KRT_IR_ADD,
    KRT_IR_SUB,
    KRT_IR_MUL,
    KRT_IR_DIV,
    KRT_IR_MOD,
    KRT_IR_AND,
    KRT_IR_OR,
    KRT_IR_XOR,
    KRT_IR_LSHIFT,
    KRT_IR_RSHIFT,
    KRT_IR_POW,
    KRT_IR_LT,
    KRT_IR_GT,
    KRT_IR_EQ,
    KRT_IR_LE,
    KRT_IR_GE,
    KRT_IR_NE,
    KRT_IR_JUMP,
    KRT_IR_BRANCH,
    KRT_IR_CALL,
    KRT_IR_RETURN,
    KRT_IR_LABEL,
    KRT_IR_STRCAT,
    KRT_IR_CAST,
    KRT_IR_LOADPTR,
    KRT_IR_STOREPTR,
    KRT_IR_ARRAY_STORE,
    KRT_IR_INT_TO_STRING,
    KRT_IR_DOUBLE_TO_STRING,
    KRT_IR_COPY,   
    KRT_IR_NOP,
} KrtIROpcode;

#define KRT_IR_CACHE_LINE_SIZE 64

typedef struct KrtIRInst {
    
    KrtIROpcode opcode;           
    int operand_count;           
    int operand_capacity;        
    bool is_int_result;          
    char _padding[3];            
    
    KrtIRValue* operands;         
    KrtIRValue result;            
    
    KrtIRInst* next;              
} KrtIRInst;

#define KRT_IR_BLOCK_CACHE_SIZE 4

typedef struct KrtIRBasicBlock {
    
    char* label;                 
    int id;                      
    int inst_count;              
    int inst_capacity;           
    int cache_count;             
    
    KrtIRInst** insts;            
    KrtIRInst* cache[KRT_IR_BLOCK_CACHE_SIZE];  
    
    KrtIRInst* first_inst;        
    KrtIRInst* last_inst;         
    
    struct KrtIRBasicBlock** preds;  
    int pred_count;
    int pred_capacity;
    struct KrtIRBasicBlock** succs;  
    int succ_count;
    int succ_capacity;
    
    struct KrtIRBasicBlock* next;  
} KrtIRBasicBlock;

typedef struct {
    char* name;
    KrtTokenType type;
} KrtIRParam;

typedef struct {
    char* name;
    KrtTokenType type;
    int has_initializer;
    double init_number;
} KrtIRGlobal;

typedef struct KrtIRFunction {
    char* name;
    KrtIRParam* params;              
    KrtIRParamTable* param_table;    
    int param_count;
    KrtTokenType return_type;
    KrtIRBasicBlock* entry_block;
    KrtIRBasicBlock* exit_block;
    int stack_size;
    int has_calls;
    struct KrtIRFunction* next;
} KrtIRFunction;

typedef struct KrtIRModule {
    KrtIRFunction* functions;
    KrtIRFunction* main_function;
    char** string_constants;
    int string_const_count;
    int string_const_capacity;
    KrtIRGlobal* globals;
    int global_count;
    int global_capacity;
} KrtIRModule;

struct KrtIRBuilder {
    KrtIRModule* module;
    KrtIRFunction* current_function;
    KrtIRBasicBlock* current_block;
    int temp_counter;
    int label_counter;
    KrtIRBasicBlock** loop_continue_blocks;
    KrtIRBasicBlock** loop_break_blocks;
    int loop_stack_size;
    int loop_stack_capacity;
    char** class_name_stack;
    int class_stack_size;
    int class_stack_capacity;
    char** namespace_stack;
    int namespace_stack_size;
    int namespace_stack_capacity;
    struct KrtIRClassLayout* layouts;
    int layout_count;
    int layout_capacity;
    struct TypeCheckContext* type_context;
    KrtIRMemoryArena* arena;
    
    int use_object_pool;
    int use_lazy_alloc;
};
KrtIRBuilder* KrtIrBuilderCreate(void);
void KrtIrBuilderDestroy(KrtIRBuilder* builder);
KrtIRModule* KrtIrModuleCreate(void);
void KrtIrModuleDestroy(KrtIRModule* module);
KrtIRFunction* KrtIrFunctionCreate(KrtIRBuilder* builder, const char* name, KrtIRParam* params, int param_count, KrtTokenType return_type);
void KrtIrFunctionSetEntry(KrtIRBuilder* builder, KrtIRFunction* func);
KrtIRBasicBlock* KrtIrBlockCreate(KrtIRBuilder* builder, const char* label);
void KrtIrBlockSetCurrent(KrtIRBuilder* builder, KrtIRBasicBlock* block);

int KrtIrBlockGetInstCount(KrtIRBasicBlock* block);
KrtIRInst* KrtIrBlockGetInst(KrtIRBasicBlock* block, int index);
KrtIRInst* KrtIrBlockGetFirstInst(KrtIRBasicBlock* block);
KrtIRInst* KrtIrBlockGetLastInst(KrtIRBasicBlock* block);

void KrtIrBlockAddPred(KrtIRBuilder* builder, KrtIRBasicBlock* block, KrtIRBasicBlock* pred);
void KrtIrBlockAddSucc(KrtIRBuilder* builder, KrtIRBasicBlock* block, KrtIRBasicBlock* succ);
int KrtIrBlockGetPredCount(KrtIRBasicBlock* block);
int KrtIrBlockGetSuccCount(KrtIRBasicBlock* block);
KrtIRBasicBlock* KrtIrBlockGetPred(KrtIRBasicBlock* block, int index);
KrtIRBasicBlock* KrtIrBlockGetSucc(KrtIRBasicBlock* block, int index);

void KrtIrBlockInvalidateCache(KrtIRBasicBlock* block);
KrtIRInst* KrtIrBlockFindCachedInst(KrtIRBasicBlock* block, KrtIROpcode opcode);
KrtIRValue KrtIrLoad(KrtIRBuilder* builder, const char* name);
void KrtIrStore(KrtIRBuilder* builder, const char* name, KrtIRValue value);
void KrtIrAlloc(KrtIRBuilder* builder, const char* name);
KrtIRValue KrtIrLoadPtr(KrtIRBuilder* builder, KrtIRValue base, int offset);
void KrtIrStorePtr(KrtIRBuilder* builder, KrtIRValue base, int offset, KrtIRValue value);
void KrtIrArrayStore(KrtIRBuilder* builder, KrtIRValue array, KrtIRValue index, KrtIRValue value);
KrtIRValue KrtIrAdd(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrSub(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrMul(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrDiv(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrMod(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrAnd(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrOr(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrXor(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrLshift(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrRshift(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrPow(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrStrcat(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs);
KrtIRValue KrtIrIntToString(KrtIRBuilder* builder, KrtIRValue value);
KrtIRValue KrtIrDoubleToString(KrtIRBuilder* builder, KrtIRValue value);
KrtIRValue KrtIrCast(KrtIRBuilder* builder, KrtIRValue value, KrtTokenType target_type);
KrtIRValue KrtIrCompare(KrtIRBuilder* builder, KrtIROpcode op, KrtIRValue lhs, KrtIRValue rhs);
void KrtIrJump(KrtIRBuilder* builder, KrtIRBasicBlock* target);
void KrtIrBranch(KrtIRBuilder* builder, KrtIRValue cond, KrtIRBasicBlock* true_block, KrtIRBasicBlock* false_block);
KrtIRValue KrtIrCall(KrtIRBuilder* builder, const char* func_name, KrtIRValue* args, int arg_count);
void KrtIrReturn(KrtIRBuilder* builder, KrtIRValue value);
void KrtIrLabel(KrtIRBuilder* builder, const char* label);
void KrtIrNop(KrtIRBuilder* builder);
void KrtIrPushLoopContext(KrtIRBuilder* builder, KrtIRBasicBlock* continue_block, KrtIRBasicBlock* break_block);
void KrtIrPopLoopContext(KrtIRBuilder* builder);
KrtIRBasicBlock* KrtIrGetCurrentContinueBlock(KrtIRBuilder* builder);
KrtIRBasicBlock* KrtIrGetCurrentBreakBlock(KrtIRBuilder* builder);
KrtIRValue KrtIrImm(KrtIRBuilder* builder, double value);
KrtIRValue KrtIrVar(KrtIRBuilder* builder, const char* name);
KrtIRValue KrtIrTemp(KrtIRBuilder* builder);
KrtIRValue KrtIrArg(KrtIRBuilder* builder, int index);
KrtIRValue KrtIrStringConst(KrtIRBuilder* builder, const char* str);
KrtIRGlobal* KrtIrModuleAddGlobal(KrtIRBuilder* builder, const char* name, KrtTokenType type);
KrtIRGlobal* KrtIrModuleFindGlobal(KrtIRModule* module, const char* name);
void KrtIrModuleSetGlobalNumberInitializer(KrtIRGlobal* global, double value);
void KrtIrGenerateFromAst(KrtIRBuilder* builder, ASTNode* ast, struct TypeCheckContext* type_context);
void KrtIrPrint(KrtIRModule* module, FILE* output);

typedef struct KrtIRFieldOffset {
    char* name;
    int offset;
} KrtIRFieldOffset;

typedef struct KrtIRClassLayout {
    char* class_name;
    KrtIRFieldOffset* fields;
    int field_count;
    int field_capacity;
} KrtIRClassLayout;
void KrtIrRegisterClassLayout(KrtIRBuilder* builder, const char* class_name, ASTNode* class_body);
int KrtIrLayoutGetOffset(KrtIRBuilder* builder, const char* class_name, const char* field_name);
int KrtIrLayoutGetSize(KrtIRBuilder* builder, const char* class_name);

KrtIRParamNode* KrtIrFunctionFindParam(KrtIRFunction* func, const char* name);
int KrtIrFunctionGetParamIndex(KrtIRFunction* func, const char* name);

KRT_IR_INLINE KrtIRValue KrtIrImmFast(double value) {
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_IMM;
    result.data.imm = value;
    return result;
}

KRT_IR_INLINE KrtIRValue KrtIrTempFast(int* counter) {
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_TEMP;
    result.data.index = (*counter)++;
    return result;
}

KRT_IR_INLINE KrtIRValue KrtIrArgFast(int index) {
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_ARG;
    result.data.index = index;
    return result;
}

KRT_IR_INLINE int KrtIrBlockInstCountFast(KrtIRBasicBlock* block) {
    return KRT_IR_LIKELY(block != NULL) ? block->inst_count : 0;
}

KRT_IR_INLINE KrtIRInst* KrtIrBlockInstFast(KrtIRBasicBlock* block, int index) {
    if (KRT_IR_UNLIKELY(block == NULL || index < 0 || index >= block->inst_count)) {
        return NULL;
    }
    return block->insts[index];
}

KRT_IR_INLINE KrtIROpcode KrtIrInstOpcodeFast(KrtIRInst* inst) {
    return KRT_IR_LIKELY(inst != NULL) ? inst->opcode : KRT_IR_NOP;
}

KRT_IR_INLINE int KrtIrInstOperandCountFast(KrtIRInst* inst) {
    return KRT_IR_LIKELY(inst != NULL) ? inst->operand_count : 0;
}

KRT_IR_INLINE KrtIRValue* KrtIrInstOperandsFast(KrtIRInst* inst) {
    return KRT_IR_LIKELY(inst != NULL) ? inst->operands : NULL;
}

#endif  