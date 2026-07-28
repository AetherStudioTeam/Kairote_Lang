#include "Ir.h"
#include "IrMemory.h"
#include "IrAssert.h"
#include "IrObjectPool.h"
#include "IrLazyAlloc.h"
#include "../../../Core/Utils/KrtCommon.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern const size_t g_object_sizes[KRT_POOL_COUNT];
extern const char* g_pool_names[KRT_POOL_COUNT];

typedef struct {
    KrtIRPoolManager* pool_manager;
    KrtLazyAllocManager* lazy_manager;
} KrtIRBuilderExtensions;

static KrtIRBuilderExtensions* get_extensions(KrtIRBuilder* builder) {
    if (!builder) return NULL;
    
    static KrtIRBuilderExtensions extensions = {0};
    static int initialized = 0;
    
    if (!initialized) {
        extensions.pool_manager = KRT_CALLOC(1, sizeof(KrtIRPoolManager));
        if (extensions.pool_manager) {
            extensions.pool_manager->arena = NULL;
            for (int i = 0; i < KRT_POOL_COUNT; i++) {
                pool_init(&extensions.pool_manager->pools[i], g_pool_names[i], g_object_sizes[i], 64);
            }
        }
        
        extensions.lazy_manager = KRT_CALLOC(1, sizeof(KrtLazyAllocManager));
        if (extensions.lazy_manager) {
            KrtIrLazyInit(extensions.lazy_manager);
        }
        
        initialized = 1;
    }
    
    return &extensions;
}

extern int strcmp(const char *s1, const char *s2);
extern void *memset(void *s, int c, size_t n);

KRT_IR_INLINE KrtIRInst* ir_create_inst(KrtIRBuilder* builder) {
    if (builder->use_object_pool) {
        KrtIRBuilderExtensions* ext = get_extensions(builder);
        if (ext && ext->pool_manager) {
            KrtIRObjectPool* pool = &ext->pool_manager->pools[KRT_POOL_INST];
            void* obj = pool_alloc(pool);
            if (!obj) {
                pool_grow(pool, builder->arena);
                obj = pool_alloc(pool);
            }
            return (KrtIRInst*)obj;
        }
    }
    return (KrtIRInst*)KrtIrArenaAlloc(builder->arena, sizeof(KrtIRInst));
}

KRT_IR_INLINE KrtIRBasicBlock* ir_create_block(KrtIRBuilder* builder) {
    if (builder->use_object_pool) {
        KrtIRBuilderExtensions* ext = get_extensions(builder);
        if (ext && ext->pool_manager) {
            KrtIRObjectPool* pool = &ext->pool_manager->pools[KRT_POOL_BLOCK];
            void* obj = pool_alloc(pool);
            if (!obj) {
                pool_grow(pool, builder->arena);
                obj = pool_alloc(pool);
            }
            return (KrtIRBasicBlock*)obj;
        }
    }
    return (KrtIRBasicBlock*)KrtIrArenaAlloc(builder->arena, sizeof(KrtIRBasicBlock));
}

KRT_IR_INLINE KrtIRFunction* ir_create_function(KrtIRBuilder* builder) {
    if (builder->use_object_pool) {
        KrtIRBuilderExtensions* ext = get_extensions(builder);
        if (ext && ext->pool_manager) {
            KrtIRObjectPool* pool = &ext->pool_manager->pools[KRT_POOL_VALUE];
            void* obj = pool_alloc(pool);
            if (!obj) {
                pool_grow(pool, builder->arena);
                obj = pool_alloc(pool);
            }
            return (KrtIRFunction*)obj;
        }
    }
    return (KrtIRFunction*)KrtIrArenaAlloc(builder->arena, sizeof(KrtIRFunction));
}

KrtIRValue KrtIrImm(KrtIRBuilder* builder, double value) {
    (void)builder;
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_IMM;
    result.data.imm = value;
    return result;
}

KrtIRValue KrtIrVar(KrtIRBuilder* builder, const char* name) {
    KRT_IR_ASSERT_VALID_BUILDER(builder);
    KRT_IR_ASSERT_NOT_NULL(name);
    
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_VAR;
    result.data.name = KrtIrArenaStrdup(builder->arena, name);
    if (!result.data.name) {
        KrtError("Failed to duplicate variable name '%s'", name);
        
        result.type = KRT_IR_VALUE_VOID;
    }
    return result;
}

KrtIRValue KrtIrTemp(KrtIRBuilder* builder) {
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_TEMP;
    result.data.index = builder->temp_counter++;
    return result;
}

KrtIRValue KrtIrArg(KrtIRBuilder* builder, int index) {
    (void)builder;
    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_ARG;
    result.data.index = index;
    return result;
}

KrtIRValue KrtIrStringConst(KrtIRBuilder* builder, const char* str) {
    if (!builder || !builder->arena || !str) {
        KrtIRValue result = {0};
        return result;
    }

    for (int i = 0; i < builder->module->string_const_count; i++) {
        if (strcmp(builder->module->string_constants[i], str) == 0) {
            KrtIRValue result = {0};
            result.type = KRT_IR_VALUE_STRING_CONST;
            result.data.string_const_id = i;
            return result;
        }
    }

    if (builder->module->string_const_count >= builder->module->string_const_capacity) {
        builder->module->string_const_capacity *= 2;
        builder->module->string_constants = KRT_REALLOC(builder->module->string_constants,
            builder->module->string_const_capacity * sizeof(char*));
    }

    int id = builder->module->string_const_count++;
    builder->module->string_constants[id] = KrtIrArenaStrdup(builder->arena, str);

    KrtIRValue result = {0};
    result.type = KRT_IR_VALUE_STRING_CONST;
    result.data.string_const_id = id;
    return result;
}

KrtIRModule* KrtIrModuleCreate(void) {
    KrtIRModule* module = KRT_CALLOC(1, sizeof(KrtIRModule));
    module->string_const_capacity = 16;
    module->string_constants = KRT_CALLOC(module->string_const_capacity, sizeof(char*));
    module->global_capacity = 8;
    module->globals = KRT_CALLOC(module->global_capacity, sizeof(KrtIRGlobal));
    return module;
}

void KrtIrModuleDestroy(KrtIRModule* module) {
    if (!module) return;

    KRT_FREE(module->string_constants);

    KRT_FREE(module->globals);

    KRT_FREE(module);
}

static KrtIRGlobal* KrtIrModuleFindGlobalInternal(KrtIRModule* module, const char* name) {
    if (!module || !name) return NULL;
    for (int i = 0; i < module->global_count; i++) {
        if (module->globals[i].name && strcmp(module->globals[i].name, name) == 0) {
            return &module->globals[i];
        }
    }
    return NULL;
}

KrtIRGlobal* KrtIrModuleFindGlobal(KrtIRModule* module, const char* name) {
    return KrtIrModuleFindGlobalInternal(module, name);
}

KrtIRGlobal* KrtIrModuleAddGlobal(KrtIRBuilder* builder, const char* name, KrtTokenType type) {
    if (!builder || !builder->module || !name) return NULL;

    KrtIRModule* module = builder->module;
    KrtIRGlobal* existing = KrtIrModuleFindGlobalInternal(module, name);
    if (existing) {
        return existing;
    }

    if (module->global_count >= module->global_capacity) {
        int old_capacity = module->global_capacity;
        module->global_capacity = old_capacity > 0 ? old_capacity * 2 : 8;
        module->globals = KRT_REALLOC(module->globals, module->global_capacity * sizeof(KrtIRGlobal));
        memset(module->globals + old_capacity, 0, (module->global_capacity - old_capacity) * sizeof(KrtIRGlobal));
    }

    KrtIRGlobal* global = &module->globals[module->global_count++];
    global->name = KRT_STRDUP(name);
    global->type = type;
    global->has_initializer = 0;
    global->init_number = 0;
    return global;
}

void KrtIrModuleSetGlobalNumberInitializer(KrtIRGlobal* global, double value) {
    if (!global) return;
    global->has_initializer = 1;
    global->init_number = value;
}

KrtIRBuilder* KrtIrBuilderCreate(void) {
    KrtIRBuilder* builder = KRT_CALLOC(1, sizeof(KrtIRBuilder));
    if (!builder) {
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate KrtIRBuilder");
    }

    builder->arena = KrtIrArenaCreate(DEFAULT_POOL_SIZE);
    if (!builder->arena) {
        KrtError("Failed to create IR memory arena");
        KRT_FREE(builder);
        return NULL;
    }

    builder->module = KrtIrModuleCreate();
    if (!builder->module) {
        KrtError("Failed to create IR module");
        KrtIrArenaDestroy(builder->arena);
        KRT_FREE(builder);
        return NULL;
    }

    builder->temp_counter = 0;
    builder->label_counter = 0;
    builder->loop_stack_size = 0;
    builder->loop_stack_capacity = 8;
    builder->loop_continue_blocks = KRT_CALLOC(builder->loop_stack_capacity, sizeof(KrtIRBasicBlock*));
    if (!builder->loop_continue_blocks) {
        KrtError("Failed to allocate loop continue blocks array");
        goto cleanup;
    }
    builder->loop_break_blocks = KRT_CALLOC(builder->loop_stack_capacity, sizeof(KrtIRBasicBlock*));
    if (!builder->loop_break_blocks) {
        KrtError("Failed to allocate loop break blocks array");
        goto cleanup;
    }

    builder->class_stack_size = 0;
    builder->class_stack_capacity = 4;
    builder->class_name_stack = KRT_CALLOC(builder->class_stack_capacity, sizeof(char*));
    if (!builder->class_name_stack) {
        KrtError("Failed to allocate class name stack");
        goto cleanup;
    }

    builder->namespace_stack_size = 0;
    builder->namespace_stack_capacity = 8;
    builder->namespace_stack = KRT_CALLOC(builder->namespace_stack_capacity, sizeof(char*));
    if (!builder->namespace_stack) {
        KrtError("Failed to allocate namespace stack");
        goto cleanup;
    }

    builder->layout_capacity = 4;
    builder->layout_count = 0;
    builder->layouts = KRT_CALLOC(builder->layout_capacity, sizeof(KrtIRClassLayout));
    if (!builder->layouts) {
        KrtError("Failed to allocate layouts array");
        goto cleanup;
    }

    builder->use_object_pool = 0;  
    builder->use_lazy_alloc = 1;

    return builder;

cleanup:
    if (builder) {
        if (builder->loop_continue_blocks) KRT_FREE(builder->loop_continue_blocks);
        if (builder->loop_break_blocks) KRT_FREE(builder->loop_break_blocks);
        if (builder->class_name_stack) KRT_FREE(builder->class_name_stack);
        if (builder->namespace_stack) KRT_FREE(builder->namespace_stack);
        if (builder->layouts) KRT_FREE(builder->layouts);
        if (builder->arena) KrtIrArenaDestroy(builder->arena);
        KRT_FREE(builder);
    }
    return NULL;
}

void KrtIrBuilderDestroy(KrtIRBuilder* builder) {
    if (!builder) return;
    
    KrtIrModuleDestroy(builder->module);
    
    if (builder->arena) {
        KrtIrArenaDestroy(builder->arena);
    }

    if (builder->loop_continue_blocks) {
        KRT_FREE(builder->loop_continue_blocks);
    }
    if (builder->loop_break_blocks) {
        KRT_FREE(builder->loop_break_blocks);
    }

    if (builder->class_name_stack) {
        for (int i = 0; i < builder->class_stack_size; ++i) {
            KRT_FREE(builder->class_name_stack[i]);
        }
        KRT_FREE(builder->class_name_stack);
    }

    if (builder->namespace_stack) {
        for (int i = 0; i < builder->namespace_stack_size; ++i) {
            KRT_FREE(builder->namespace_stack[i]);
        }
        KRT_FREE(builder->namespace_stack);
    }
    KRT_FREE(builder);
}

KrtIRFunction* KrtIrFunctionCreate(KrtIRBuilder* builder, const char* name, KrtIRParam* params, int param_count, KrtTokenType return_type) {
    KRT_IR_ASSERT_VALID_BUILDER(builder);
    KRT_IR_ASSERT_NOT_NULL(name);
    
    KRT_IR_ASSERT(param_count >= -1);
    
    if (param_count > 0) {
        KRT_IR_ASSERT_NOT_NULL(params);
    }

    KrtIRFunction* existing = builder->module->functions;
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            if (existing->param_count == -1 && param_count >= 0) {
                existing->param_count = param_count;
                if (param_count > 0) {
                    KRT_IR_ALLOC_ARRAY_CHECKED(existing->params, KrtIRParam, param_count, builder->arena);
                    
                    if (!existing->param_table) {
                        existing->param_table = KrtIrParamTableCreate(builder->arena, param_count > 8 ? param_count : 8);
                        if (!existing->param_table) {
                            KRT_IR_HANDLE_ALLOC_FAIL("Failed to create param table");
                        }
                    }
                    
                    for (int i = 0; i < param_count; i++) {
                        KRT_IR_STRDUP_CHECKED(existing->params[i].name, params[i].name, builder->arena);
                        existing->params[i].type = params[i].type;
                        KrtIrParamTableAdd(existing->param_table, params[i].name, params[i].type, i);
                    }
                } else {
                    existing->params = NULL;
                }
            }
            return existing;
        }
        existing = existing->next;
    }

    KrtIRFunction* func = ir_create_function(builder);
    if (!func) {
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate KrtIRFunction");
    }
    
    func->name = NULL;
    func->params = NULL;
    func->param_table = NULL;
    func->param_count = 0;
    func->return_type = TOKEN_VOID;
    func->entry_block = NULL;
    func->exit_block = NULL;
    func->stack_size = 0;
    func->has_calls = 0;
    func->next = NULL;
    
    KRT_IR_STRDUP_CHECKED(func->name, name, builder->arena);
    func->param_count = param_count;
    func->return_type = return_type;

    if (param_count >= 0) {
        func->param_table = KrtIrParamTableCreate(builder->arena, param_count > 8 ? param_count : 8);
        if (!func->param_table && param_count > 0) {
            KRT_IR_HANDLE_ALLOC_FAIL("Failed to create param table");
        }
    }

    if (param_count > 0) {
        KRT_IR_ALLOC_ARRAY_CHECKED(func->params, KrtIRParam, param_count, builder->arena);
        for (int i = 0; i < param_count; i++) {
            KRT_IR_STRDUP_CHECKED(func->params[i].name, params[i].name, builder->arena);
            func->params[i].type = params[i].type;
            KrtIrParamTableAdd(func->param_table, params[i].name, params[i].type, i);
        }
    }

    if (!builder->module->functions) {
        builder->module->functions = func;
    } else {
        KrtIRFunction* last = builder->module->functions;
        while (last->next) {
            last = last->next;
        }
        last->next = func;
    }

    if (strcmp(name, "main") == 0) {
        builder->module->main_function = func;
    }

    return func;
}

void KrtIrFunctionSetEntry(KrtIRBuilder* builder, KrtIRFunction* func) {
    builder->current_function = func;

}

static int g_block_id_counter = 0;

KrtIRBasicBlock* KrtIrBlockCreate(KrtIRBuilder* builder, const char* label) {
    KRT_IR_ASSERT_VALID_BUILDER(builder);
    KRT_IR_ASSERT_NOT_NULL(label);
    
    KrtIRBasicBlock* block = ir_create_block(builder);
    if (!block) {
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate KrtIRBasicBlock");
    }
    
    memset(block, 0, sizeof(KrtIRBasicBlock));
    KRT_IR_STRDUP_CHECKED(block->label, label, builder->arena);
    block->id = g_block_id_counter++;
    
    block->inst_capacity = 16;
    KRT_IR_ALLOC_ARRAY_CHECKED(block->insts, KrtIRInst*, block->inst_capacity, builder->arena);
    block->inst_count = 0;
    
    block->pred_capacity = 4;
    KRT_IR_ALLOC_ARRAY_CHECKED(block->preds, KrtIRBasicBlock*, block->pred_capacity, builder->arena);
    block->pred_count = 0;
    
    block->succ_capacity = 4;
    KRT_IR_ALLOC_ARRAY_CHECKED(block->succs, KrtIRBasicBlock*, block->succ_capacity, builder->arena);
    block->succ_count = 0;
    
    block->cache_count = 0;
    
    return block;
}

void KrtIrBlockSetCurrent(KrtIRBuilder* builder, KrtIRBasicBlock* block) {
    if (!block) return;

    builder->current_block = block;

    if (builder->current_function && !builder->current_function->entry_block) {
        builder->current_function->entry_block = block;
    } else if (builder->current_function && builder->current_function->entry_block) {
        
        KrtIRBasicBlock* check = builder->current_function->entry_block;
        while (check) {
            if (check == block) {
                
                return;
            }
            check = check->next;
        }

        KrtIRBasicBlock* last = builder->current_function->entry_block;
        int count = 0;
        while (last->next) {
            if (count++ > 10000) {
                
                return;
            }
            last = last->next;
        }
        if (last != block) {
            last->next = block;
        }
    }
}

static inline int get_operand_capacity(KrtIROpcode opcode) {
    switch (opcode) {
        
        case KRT_IR_NOP:
            return 0;
        
        case KRT_IR_LOAD:
        case KRT_IR_ALLOC:
        case KRT_IR_RETURN:
        case KRT_IR_JUMP:
        case KRT_IR_IMM:
        case KRT_IR_INT_TO_STRING:
        case KRT_IR_DOUBLE_TO_STRING:
            return 1;
        
        case KRT_IR_STORE:
        case KRT_IR_ADD:
        case KRT_IR_SUB:
        case KRT_IR_MUL:
        case KRT_IR_DIV:
        case KRT_IR_MOD:
        case KRT_IR_AND:
        case KRT_IR_OR:
        case KRT_IR_XOR:
        case KRT_IR_LSHIFT:
        case KRT_IR_RSHIFT:
        case KRT_IR_POW:
        case KRT_IR_LT:
        case KRT_IR_GT:
        case KRT_IR_EQ:
        case KRT_IR_LE:
        case KRT_IR_GE:
        case KRT_IR_NE:
        case KRT_IR_STRCAT:
        case KRT_IR_CAST:
        case KRT_IR_COPY:
        case KRT_IR_LOADPTR:
            return 2;
        
        case KRT_IR_STOREPTR:
        case KRT_IR_BRANCH:
        case KRT_IR_ARRAY_STORE:
            return 3;
        
        case KRT_IR_CALL:
            return 16;
        
        default:
            return 4;
    }
}

static KrtIRInst* add_instruction(KrtIRBuilder* builder, KrtIROpcode opcode) {
    KRT_IR_ASSERT_VALID_BUILDER(builder);
    KRT_IR_ASSERT(opcode >= KRT_IR_LOAD && opcode <= KRT_IR_NOP);
    
    if (!builder->current_block) {
        char label[32];
        snprintf(label, sizeof(label), "block_%d", builder->label_counter++);
        KrtIRBasicBlock* block = KrtIrBlockCreate(builder, label);
        KrtIrBlockSetCurrent(builder, block);
    }
    
    KRT_IR_ASSERT_NOT_NULL(builder->current_block);

    KrtIRInst* inst = ir_create_inst(builder);
    if (!inst) {
        KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate KrtIRInst");
    }
    memset(inst, 0, sizeof(KrtIRInst));
    inst->opcode = opcode;
    
    inst->operand_capacity = get_operand_capacity(opcode);
    if (inst->operand_capacity > 0) {
        inst->operands = (KrtIRValue*)KrtIrArenaAlloc(builder->arena, inst->operand_capacity * sizeof(KrtIRValue));
        if (!inst->operands) {
            KRT_IR_HANDLE_ALLOC_FAIL("Failed to allocate operand array");
        }
    } else {
        inst->operands = NULL;
    }

    KrtIRBasicBlock* block = builder->current_block;
    
    if (block->inst_count >= block->inst_capacity) {
        
        int new_capacity = block->inst_capacity * 2;
        KrtIRInst** new_insts = (KrtIRInst**)KRT_MALLOC(new_capacity * sizeof(KrtIRInst*));
        if (!new_insts) return NULL;
        memcpy(new_insts, block->insts, block->inst_count * sizeof(KrtIRInst*));
        block->insts = new_insts;
        block->inst_capacity = new_capacity;
    }
    
    block->insts[block->inst_count++] = inst;
    
    if (!block->first_inst) {
        block->first_inst = inst;
        block->last_inst = inst;
    } else {
        block->last_inst->next = inst;
        block->last_inst = inst;
    }
    
    if (block->cache_count < KRT_IR_BLOCK_CACHE_SIZE) {
        
        if (opcode == KRT_IR_LOAD || opcode == KRT_IR_STORE || 
            opcode == KRT_IR_ADD || opcode == KRT_IR_SUB ||
            opcode == KRT_IR_MUL || opcode == KRT_IR_DIV) {
            block->cache[block->cache_count++] = inst;
        }
    }

    return inst;
}

static void add_operand(KrtIRInst* inst, KrtIRValue operand) {
    KRT_IR_ASSERT_VALID_INST(inst);
    KRT_IR_ASSERT_VALID_VALUE(operand);
    
    if (KRT_IR_LIKELY(inst->operand_count < inst->operand_capacity)) {
        inst->operands[inst->operand_count++] = operand;
    } else {
        
        KRT_IR_ASSERT_MSG(0, "Operand overflow - increase preallocation");
    }
}

KrtIRValue KrtIrLoad(KrtIRBuilder* builder, const char* name) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_LOAD);
    add_operand(inst, KrtIrVar(builder, name));
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

void KrtIrStore(KrtIRBuilder* builder, const char* name, KrtIRValue value) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_STORE);
    add_operand(inst, KrtIrVar(builder, name));
    add_operand(inst, value);
}

void KrtIrAlloc(KrtIRBuilder* builder, const char* name) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_ALLOC);
    add_operand(inst, KrtIrVar(builder, name));
}

KrtIRValue KrtIrLoadPtr(KrtIRBuilder* builder, KrtIRValue base, int offset) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_LOADPTR);
    add_operand(inst, base);
    add_operand(inst, KrtIrImm(builder, offset));
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

void KrtIrStorePtr(KrtIRBuilder* builder, KrtIRValue base, int offset, KrtIRValue value) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_STOREPTR);
    add_operand(inst, base);
    add_operand(inst, KrtIrImm(builder, offset));
    add_operand(inst, value);
}

void KrtIrArrayStore(KrtIRBuilder* builder, KrtIRValue array, KrtIRValue index, KrtIRValue value) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_ARRAY_STORE);
    add_operand(inst, array);
    add_operand(inst, index);
    add_operand(inst, value);
}

KrtIRValue KrtIrAdd(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_ADD);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrSub(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_SUB);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrMul(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_MUL);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrDiv(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_DIV);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrMod(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_MOD);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrAnd(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_AND);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrOr(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_OR);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrXor(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_XOR);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrLshift(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_LSHIFT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrRshift(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_RSHIFT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrPow(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    KrtIRInst* inst = add_instruction(builder, KRT_IR_POW);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrStrcat(KrtIRBuilder* builder, KrtIRValue lhs, KrtIRValue rhs) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    KrtIRInst* inst = add_instruction(builder, KRT_IR_STRCAT);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    
    return inst->result;
}

KrtIRValue KrtIrIntToString(KrtIRBuilder* builder, KrtIRValue value) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    KrtIRInst* inst = add_instruction(builder, KRT_IR_INT_TO_STRING);
    add_operand(inst, value);
    inst->result = KrtIrTemp(builder);
    
    return inst->result;
}

KrtIRValue KrtIrDoubleToString(KrtIRBuilder* builder, KrtIRValue value) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    KrtIRInst* inst = add_instruction(builder, KRT_IR_DOUBLE_TO_STRING);
    add_operand(inst, value);
    inst->result = KrtIrTemp(builder);
    
    return inst->result;
}

KrtIRValue KrtIrCast(KrtIRBuilder* builder, KrtIRValue value, KrtTokenType target_type) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_CAST);
    add_operand(inst, value);

    KrtIRValue type_value = {0};
    type_value.type = KRT_IR_VALUE_IMM;
    type_value.data.imm = (double)target_type;
    add_operand(inst, type_value);

    inst->result = KrtIrTemp(builder);
    return inst->result;
}

KrtIRValue KrtIrCompare(KrtIRBuilder* builder, KrtIROpcode op, KrtIRValue lhs, KrtIRValue rhs) {
    KrtIRInst* inst = add_instruction(builder, op);
    add_operand(inst, lhs);
    add_operand(inst, rhs);
    inst->result = KrtIrTemp(builder);
    return inst->result;
}

void KrtIrJump(KrtIRBuilder* builder, KrtIRBasicBlock* target) {
    if (!builder || !target) return;

    KrtIRInst* inst = add_instruction(builder, KRT_IR_JUMP);
    if (inst) {
        inst->operand_count = 1;
        inst->operands[0].type = KRT_IR_VALUE_VAR;
        inst->operands[0].data.name = KrtIrArenaStrdup(builder->arena, target->label);
    }
}

void KrtIrBranch(KrtIRBuilder* builder, KrtIRValue cond, KrtIRBasicBlock* true_block, KrtIRBasicBlock* false_block) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_BRANCH);
    add_operand(inst, cond);

    KrtIRValue true_val = {0};
    true_val.type = KRT_IR_VALUE_VAR;
    true_val.data.name = KrtIrArenaStrdup(builder->arena, true_block->label);
    add_operand(inst, true_val);

    KrtIRValue false_val = {0};
    false_val.type = KRT_IR_VALUE_VAR;
    false_val.data.name = KrtIrArenaStrdup(builder->arena, false_block->label);
    add_operand(inst, false_val);
}

KrtIRValue KrtIrCall(KrtIRBuilder* builder, const char* func_name, KrtIRValue* args, int arg_count) {
    if (builder && builder->current_function) {
        builder->current_function->has_calls = 1;
    }
    KrtIRInst* inst = add_instruction(builder, KRT_IR_CALL);

    KrtIRValue func_val = {0};
    func_val.type = KRT_IR_VALUE_FUNCTION;
    func_val.data.function_name = KrtIrArenaStrdup(builder->arena, func_name);
    add_operand(inst, func_val);

    for (int i = 0; i < arg_count; i++) {
        add_operand(inst, args[i]);
    }

    inst->result = KrtIrTemp(builder);
    return inst->result;
}

void KrtIrReturn(KrtIRBuilder* builder, KrtIRValue value) {
    KrtIRInst* inst = add_instruction(builder, KRT_IR_RETURN);
    add_operand(inst, value);
}

void KrtIrLabel(KrtIRBuilder* builder, const char* label) {

    KrtIRBasicBlock* block = KrtIrBlockCreate(builder, label);

    KrtIrBlockSetCurrent(builder, block);
}

void KrtIrNop(KrtIRBuilder* builder) {
    add_instruction(builder, KRT_IR_NOP);
}

void KrtIrPushLoopContext(KrtIRBuilder* builder, KrtIRBasicBlock* continue_block, KrtIRBasicBlock* break_block) {
    if (!builder) return;

    if (builder->loop_stack_size >= builder->loop_stack_capacity) {
        builder->loop_stack_capacity *= 2;
        builder->loop_continue_blocks = KRT_REALLOC(builder->loop_continue_blocks,
                                                    builder->loop_stack_capacity * sizeof(KrtIRBasicBlock*));
        builder->loop_break_blocks = KRT_REALLOC(builder->loop_break_blocks,
                                                 builder->loop_stack_capacity * sizeof(KrtIRBasicBlock*));
    }

    builder->loop_continue_blocks[builder->loop_stack_size] = continue_block;
    builder->loop_break_blocks[builder->loop_stack_size] = break_block;
    builder->loop_stack_size++;
}

void KrtIrPopLoopContext(KrtIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return;

    builder->loop_stack_size--;
    builder->loop_continue_blocks[builder->loop_stack_size] = NULL;
    builder->loop_break_blocks[builder->loop_stack_size] = NULL;
}

KrtIRBasicBlock* KrtIrGetCurrentContinueBlock(KrtIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return NULL;
    return builder->loop_continue_blocks[builder->loop_stack_size - 1];
}

KrtIRBasicBlock* KrtIrGetCurrentBreakBlock(KrtIRBuilder* builder) {
    if (!builder || builder->loop_stack_size <= 0) return NULL;
    return builder->loop_break_blocks[builder->loop_stack_size - 1];
}

int KrtIrBlockGetInstCount(KrtIRBasicBlock* block) {
    if (!block) return 0;
    return block->inst_count;
}

KrtIRInst* KrtIrBlockGetInst(KrtIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->inst_count) return NULL;
    return block->insts[index];
}

KrtIRInst* KrtIrBlockGetFirstInst(KrtIRBasicBlock* block) {
    if (!block || block->inst_count == 0) return NULL;
    return block->insts[0];
}

KrtIRInst* KrtIrBlockGetLastInst(KrtIRBasicBlock* block) {
    if (!block || block->inst_count == 0) return NULL;
    return block->insts[block->inst_count - 1];
}

void KrtIrBlockAddPred(KrtIRBuilder* builder, KrtIRBasicBlock* block, KrtIRBasicBlock* pred) {
    if (!block || !pred || !builder) return;
    
    for (int i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == pred) return;
    }
    
    if (block->pred_count >= block->pred_capacity) {
        int new_capacity = block->pred_capacity * 2;
        KrtIRBasicBlock** new_preds = (KrtIRBasicBlock**)KRT_MALLOC(new_capacity * sizeof(KrtIRBasicBlock*));
        if (!new_preds) return;
        memcpy(new_preds, block->preds, block->pred_count * sizeof(KrtIRBasicBlock*));
        block->preds = new_preds;
        block->pred_capacity = new_capacity;
    }
    
    block->preds[block->pred_count++] = pred;
}

void KrtIrBlockAddSucc(KrtIRBuilder* builder, KrtIRBasicBlock* block, KrtIRBasicBlock* succ) {
    if (!block || !succ || !builder) return;
    
    for (int i = 0; i < block->succ_count; i++) {
        if (block->succs[i] == succ) return;
    }
    
    if (block->succ_count >= block->succ_capacity) {
        int new_capacity = block->succ_capacity * 2;
        KrtIRBasicBlock** new_succs = (KrtIRBasicBlock**)KRT_MALLOC(new_capacity * sizeof(KrtIRBasicBlock*));
        if (!new_succs) return;
        memcpy(new_succs, block->succs, block->succ_count * sizeof(KrtIRBasicBlock*));
        block->succs = new_succs;
        block->succ_capacity = new_capacity;
    }
    
    block->succs[block->succ_count++] = succ;
}

int KrtIrBlockGetPredCount(KrtIRBasicBlock* block) {
    if (!block) return 0;
    return block->pred_count;
}

int KrtIrBlockGetSuccCount(KrtIRBasicBlock* block) {
    if (!block) return 0;
    return block->succ_count;
}

KrtIRBasicBlock* KrtIrBlockGetPred(KrtIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->pred_count) return NULL;
    return block->preds[index];
}

KrtIRBasicBlock* KrtIrBlockGetSucc(KrtIRBasicBlock* block, int index) {
    if (!block || index < 0 || index >= block->succ_count) return NULL;
    return block->succs[index];
}

void KrtIrBlockInvalidateCache(KrtIRBasicBlock* block) {
    if (!block) return;
    block->cache_count = 0;
    memset(block->cache, 0, sizeof(block->cache));
}

KrtIRInst* KrtIrBlockFindCachedInst(KrtIRBasicBlock* block, KrtIROpcode opcode) {
    if (!block) return NULL;
    
    for (int i = 0; i < block->cache_count; i++) {
        if (block->cache[i] && block->cache[i]->opcode == opcode) {
            return block->cache[i];
        }
    }
    return NULL;
}

static int find_block_index(KrtIRFunction* func, const char* label) {
    if (!func || !label) return -1;

    KrtIRBasicBlock* block = func->entry_block;
    int index = 0;
    while (block) {
        if (strcmp(block->label, label) == 0) {
            return index;
        }
        block = block->next;
        index++;
    }
    return -1;
}

static void KrtIrPrintInstruction(KrtIRInst* inst, FILE* output, KrtIRFunction* func) {
    if (!inst || !output) return;

    if (inst->result.type != KRT_IR_VALUE_VOID) {
        switch (inst->result.type) {
            case KRT_IR_VALUE_TEMP:
                fprintf(output, "%%%d = ", inst->result.data.index);
                break;
            case KRT_IR_VALUE_VAR:
                fprintf(output, "@%s = ", inst->result.data.name);
                break;
            default:
                break;
        }
    }

    const char* op_name = "unknown";
    switch (inst->opcode) {
        case KRT_IR_LOAD: op_name = "load"; break;
        case KRT_IR_STORE: op_name = "store"; break;
        case KRT_IR_ALLOC: op_name = "alloc"; break;
        case KRT_IR_IMM: op_name = "imm"; break;
        case KRT_IR_ADD: op_name = "add"; break;
        case KRT_IR_SUB: op_name = "sub"; break;
        case KRT_IR_MUL: op_name = "mul"; break;
        case KRT_IR_DIV: op_name = "div"; break;
        case KRT_IR_MOD: op_name = "mod"; break;
        case KRT_IR_AND: op_name = "and"; break;
        case KRT_IR_OR: op_name = "or"; break;
        case KRT_IR_XOR: op_name = "xor"; break;
        case KRT_IR_LSHIFT: op_name = "lshift"; break;
        case KRT_IR_RSHIFT: op_name = "rshift"; break;
        case KRT_IR_POW: op_name = "pow"; break;
        case KRT_IR_LT: op_name = "icmp slt"; break;
        case KRT_IR_GT: op_name = "icmp sgt"; break;
        case KRT_IR_EQ: op_name = "icmp eq"; break;
        case KRT_IR_LE: op_name = "icmp sle"; break;
        case KRT_IR_GE: op_name = "icmp sge"; break;
        case KRT_IR_NE: op_name = "icmp ne"; break;
        case KRT_IR_JUMP: op_name = "br"; break;
        case KRT_IR_BRANCH: op_name = "br"; break;
        case KRT_IR_CALL: op_name = "call"; break;
        case KRT_IR_RETURN: op_name = "ret"; break;
        case KRT_IR_LABEL: op_name = "label"; break;
        case KRT_IR_STRCAT: op_name = "strcat"; break;
        case KRT_IR_CAST: op_name = "cast"; break;
        case KRT_IR_LOADPTR: op_name = "loadptr"; break;
        case KRT_IR_STOREPTR: op_name = "storeptr"; break;
        case KRT_IR_ARRAY_STORE: op_name = "array_store"; break;
        case KRT_IR_INT_TO_STRING: op_name = "int_to_string"; break;
        case KRT_IR_DOUBLE_TO_STRING: op_name = "double_to_string"; break;
        case KRT_IR_COPY: op_name = "copy"; break;
        case KRT_IR_NOP: op_name = "nop"; break;
    }
    fprintf(output, "%s", op_name);

    if (inst->operand_count > 0) {
        fprintf(output, " ");
        for (int i = 0; i < inst->operand_count; i++) {
            if (i > 0) fprintf(output, ", ");

            if ((inst->opcode == KRT_IR_JUMP || inst->opcode == KRT_IR_BRANCH) &&
                inst->operands[i].type == KRT_IR_VALUE_VAR && func) {

                if (inst->opcode == KRT_IR_BRANCH) {
                    if (i == 0) {

                        switch (inst->operands[i].type) {
                            case KRT_IR_VALUE_IMM:
                                fprintf(output, "%.0f", inst->operands[i].data.imm);
                                break;
                            case KRT_IR_VALUE_VAR:
                                fprintf(output, "@%s", inst->operands[i].data.name);
                                break;
                            case KRT_IR_VALUE_TEMP:
                                fprintf(output, "%%%d", inst->operands[i].data.index);
                                break;
                            case KRT_IR_VALUE_ARG:
                                fprintf(output, "%%%d", inst->operands[i].data.index);
                                break;
                            default:
                                fprintf(output, "void");
                                break;
                        }
                        continue;
                    } else if (i == 1 || i == 2) {

                        int block_index = find_block_index(func, inst->operands[i].data.name);
                        if (block_index >= 0) {
                            fprintf(output, "%%%d", block_index);
                            continue;
                        }
                    }
                } else {

                    int block_index = find_block_index(func, inst->operands[i].data.name);
                    if (block_index >= 0) {
                        fprintf(output, "%%%d", block_index);
                        continue;
                    }
                }
            }

            switch (inst->operands[i].type) {
                case KRT_IR_VALUE_IMM:
                    fprintf(output, "%.0f", inst->operands[i].data.imm);
                    break;
                case KRT_IR_VALUE_VAR:
                    fprintf(output, "@%s", inst->operands[i].data.name);
                    break;
                case KRT_IR_VALUE_TEMP:
                    fprintf(output, "%%%d", inst->operands[i].data.index);
                    break;
                case KRT_IR_VALUE_ARG:
                    fprintf(output, "%%%d", inst->operands[i].data.index);
                    break;
                case KRT_IR_VALUE_STRING_CONST:
                    fprintf(output, "str_const[%d]", inst->operands[i].data.string_const_id);
                    break;
                case KRT_IR_VALUE_FUNCTION:
                    fprintf(output, "%s", inst->operands[i].data.function_name);
                    break;
                default:
                    fprintf(output, "void");
                    break;
            }
        }
    }
}

static const char* KrtTokenTypeToString(KrtTokenType type) {
    switch (type) {
        case TOKEN_VOID: return "void";
        case TOKEN_INT8: return "i8";
        case TOKEN_INT16: return "i16";
        case TOKEN_INT32: return "i32";
        case TOKEN_INT64: return "i64";
        case TOKEN_UINT8: return "u8";
        case TOKEN_UINT16: return "u16";
        case TOKEN_UINT32: return "u32";
        case TOKEN_UINT64: return "u64";
        case TOKEN_FLOAT32: return "f32";
        case TOKEN_FLOAT64: return "f64";
        case TOKEN_BOOL: return "bool";
        case TOKEN_CHAR: return "char";
        case TOKEN_STRING:
        case TOKEN_TYPE_STRING: return "str";
        default: return "unknown";
    }
}

void KrtIrPrint(KrtIRModule* module, FILE* output) {
    if (!module || !output) return;

    fprintf(output, "; Kairote Lang IR v1.0\n");
    fprintf(output, "; Generated by Kairote Lang Compiler\n\n");

    KrtIRFunction* func = module->functions;
    while (func) {
        fprintf(output, "define %s @%s(", KrtTokenTypeToString(func->return_type), func->name);

        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) fprintf(output, ", ");
            fprintf(output, "%s %%%d", KrtTokenTypeToString(func->params[i].type), i);
        }
        fprintf(output, ") {\n");

        KrtIRBasicBlock* block = func->entry_block;
        int block_index = 0;
        while (block) {
            fprintf(output, "  %%%d: ; %s (%d insts)\n", block_index++, block->label, block->inst_count);

            for (int i = 0; i < block->inst_count; i++) {
                fprintf(output, "    ");
                KrtIrPrintInstruction(block->insts[i], output, func);
                fprintf(output, "\n");
            }

            block = block->next;
        }

        fprintf(output, "}\n\n");
        func = func->next;
    }

    fflush(output);
}

static KrtIRClassLayout* ensure_class_layout(KrtIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) return NULL;
    for (int i = 0; i < builder->layout_count; i++) {
        if (builder->layouts[i].class_name && strcmp(builder->layouts[i].class_name, class_name) == 0) {
            return &builder->layouts[i];
        }
    }
    if (builder->layout_count >= builder->layout_capacity) {
        int old = builder->layout_capacity;
        builder->layout_capacity = old > 0 ? old * 2 : 4;
        builder->layouts = KRT_REALLOC(builder->layouts, builder->layout_capacity * sizeof(KrtIRClassLayout));
        memset(builder->layouts + old, 0, (builder->layout_capacity - old) * sizeof(KrtIRClassLayout));
    }
    KrtIRClassLayout* layout = &builder->layouts[builder->layout_count++];
    layout->class_name = KRT_STRDUP(class_name);
    layout->field_capacity = 8;
    layout->field_count = 0;
    layout->fields = KRT_CALLOC(layout->field_capacity, sizeof(KrtIRFieldOffset));
    return layout;
}

void KrtIrRegisterClassLayout(KrtIRBuilder* builder, const char* class_name, ASTNode* class_body) {
    if (!builder || !class_name || !class_body) return;
    KrtIRClassLayout* layout = ensure_class_layout(builder, class_name);
    if (!layout) return;
    if (class_body->type == AST_BLOCK) {
        int slot = 0;
        for (int i = 0; i < class_body->data.block.statement_count; i++) {
            ASTNode* member = class_body->data.block.statements[i];
            if (!member) continue;
            if (member->type == AST_ACCESS_MODIFIER) {
                member = member->data.access_modifier.member;
                if (!member) continue;
            }
            if (member->type == AST_VARIABLE_DECLARATION) {
                if (layout->field_count >= layout->field_capacity) {
                    int old = layout->field_capacity;
                    layout->field_capacity = old > 0 ? old * 2 : 8;
                    layout->fields = KRT_REALLOC(layout->fields, layout->field_capacity * sizeof(KrtIRFieldOffset));
                }
                layout->fields[layout->field_count].name = KRT_STRDUP(member->data.variable_decl.name);
                layout->fields[layout->field_count].offset = slot * 8;
                layout->field_count++;
                slot++;
            }
        }
    }
}

int KrtIrLayoutGetOffset(KrtIRBuilder* builder, const char* class_name, const char* field_name) {
    if (!builder || !class_name || !field_name) return -1;
    for (int i = 0; i < builder->layout_count; i++) {
        KrtIRClassLayout* layout = &builder->layouts[i];
        if (layout->class_name && strcmp(layout->class_name, class_name) == 0) {
            for (int j = 0; j < layout->field_count; j++) {
                if (layout->fields[j].name && strcmp(layout->fields[j].name, field_name) == 0) {
                    return layout->fields[j].offset;
                }
            }
        }
    }
    return -1;
}

int KrtIrLayoutGetSize(KrtIRBuilder* builder, const char* class_name) {
    if (!builder || !class_name) return 0;
    for (int i = 0; i < builder->layout_count; i++) {
        KrtIRClassLayout* layout = &builder->layouts[i];
        if (layout->class_name && strcmp(layout->class_name, class_name) == 0) {
            return layout->field_count * 8;
        }
    }
    return 8 * 8;
}