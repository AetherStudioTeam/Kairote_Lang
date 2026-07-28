#include "X86Codegen.h"
#include "X86RegAlloc.h"
#include "../../../Core/Utils/KrtCommon.h"
#include <string.h>

#define X86_STACK_ALIGNMENT 16
#define X86_MIN_STACK_SIZE 48
#define X86_SHADOW_SPACE 32
#define X86_MAX_TEMP_SPACE 256
#define X86_REGISTER_COUNT 14

static const char* g_register_names[] = {
    "rax", "rbx", "rcx", "rdx",
    "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

static int g_register_count = X86_REGISTER_COUNT;

static int calculate_stack_size(KrtIRFunction* func) {
    int stack_size = X86_SHADOW_SPACE;
    int max_temp_idx = 0;

    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        KrtIRInst* inst = block->first_inst;
        while (inst) {
            for (int i = 0; i < inst->operand_count; i++) {
                if (inst->operands[i].type == KRT_IR_VALUE_TEMP) {
                    int idx = inst->operands[i].data.index;
                    if (idx > max_temp_idx) max_temp_idx = idx;
                }
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int idx = inst->result.data.index;
                if (idx > max_temp_idx) max_temp_idx = idx;
            }
            inst = inst->next;
        }
        block = block->next;
    }

    int temp_space = (max_temp_idx + 1) * 8;
    if (temp_space > X86_MAX_TEMP_SPACE) temp_space = X86_MAX_TEMP_SPACE;

    stack_size += func->param_count * 8 + temp_space;
    stack_size = (stack_size + (X86_STACK_ALIGNMENT - 1)) & ~(X86_STACK_ALIGNMENT - 1);

    if (stack_size < X86_MIN_STACK_SIZE) stack_size = X86_MIN_STACK_SIZE;

    return stack_size;
}

void codegen_context_init(CodegenContext* ctx, KrtIRFunction* func, KrtIRModule* module) {
    if (!ctx || !func) {
        return;
    }

    memset(ctx, 0, sizeof(CodegenContext));
    ctx->current_func = func;
    ctx->current_module = module;

    for (int i = 0; i < g_register_count; i++) {
        ctx->registers[i].name = g_register_names[i];
        ctx->registers[i].is_free = 1;
        ctx->registers[i].content = NULL;
    }

    ctx->registers[0].is_free = 0;

    for (int i = 0; i < 256; i++) {
        ctx->temp_locations[i].type = TEMP_LOC_NONE;
    }

    RegAllocResult* regalloc = x86_allocate_registers(func);
    ctx->regalloc = regalloc;

    if (regalloc) {
        int reg_count = 0;
        int stack_count = 0;
        for (int i = 0; i < 256; i++) {
            const char* reg = regalloc_get_reg_name(regalloc, i);
            if (reg) {
                codegen_set_temp_in_register(ctx, i, reg);
                reg_count++;
            } else {
                int stack_offset = regalloc_get_stack_offset(regalloc, i);
                if (stack_offset > 0) {
                    codegen_set_temp_on_stack(ctx, i, stack_offset);
                    stack_count++;
                }
            }
        }
    }

    ctx->stack_size = calculate_stack_size(func);
    ctx->temp_stack_base = func->param_count * 8 + 32;  
}

TempLocation* codegen_get_temp_location(CodegenContext* ctx, int temp_idx) {
    if (temp_idx < 0 || temp_idx >= 256) return NULL;
    return &ctx->temp_locations[temp_idx];
}

void codegen_set_temp_in_register(CodegenContext* ctx, int temp_idx, const char* reg) {
    if (temp_idx < 0 || temp_idx >= 256) return;
    ctx->temp_locations[temp_idx].type = TEMP_LOC_REGISTER;
    ctx->temp_locations[temp_idx].reg = reg;
}

void codegen_set_temp_on_stack(CodegenContext* ctx, int temp_idx, int offset) {
    if (temp_idx < 0 || temp_idx >= 256) return;
    ctx->temp_locations[temp_idx].type = TEMP_LOC_STACK;
    ctx->temp_locations[temp_idx].offset = offset;
}

const char* codegen_alloc_register(CodegenContext* ctx) {
    
    for (int i = 12; i < g_register_count; i++) {
        if (ctx->registers[i].is_free) {
            ctx->registers[i].is_free = 0;
            return ctx->registers[i].name;
        }
    }
    
    for (int i = 10; i <= 11; i++) {
        if (ctx->registers[i].is_free) {
            ctx->registers[i].is_free = 0;
            return ctx->registers[i].name;
        }
    }
    
    return NULL;
}

void codegen_free_register(CodegenContext* ctx, const char* reg) {
    for (int i = 0; i < g_register_count; i++) {
        if (strcmp(ctx->registers[i].name, reg) == 0) {
            ctx->registers[i].is_free = 1;
            ctx->registers[i].content = NULL;
            return;
        }
    }
}

int codegen_get_var_offset(CodegenContext* ctx, const char* var_name) {
    
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->var_offsets[i].name, var_name) == 0) {
            return ctx->var_offsets[i].offset;
        }
    }
    
    if (ctx->var_count < 100) {
        KRT_STRCPY_S(ctx->var_offsets[ctx->var_count].name, 64, var_name);
        ctx->next_var_offset += 8;
        ctx->var_offsets[ctx->var_count].offset = ctx->next_var_offset;
        return ctx->var_offsets[ctx->var_count++].offset;
    }
    
    return 8;  
}

void codegen_emit_load_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* target_reg) {
    TempLocation* loc = codegen_get_temp_location(ctx, temp_idx);
    if (!loc) return;
    
    switch (loc->type) {
        case TEMP_LOC_REGISTER:
            if (strcmp(loc->reg, target_reg) != 0) {
                fprintf(output, "    mov %s, %s\n", target_reg, loc->reg);
            }
            break;
        case TEMP_LOC_STACK:
            fprintf(output, "    mov %s, [rbp - %d]\n", target_reg, loc->offset);
            break;
        case TEMP_LOC_NONE:
        default:
            break;
    }
}

void codegen_emit_store_temp(FILE* output, CodegenContext* ctx, int temp_idx, const char* source_reg) {
    TempLocation* loc = codegen_get_temp_location(ctx, temp_idx);
    if (!loc) return;
    
    switch (loc->type) {
        case TEMP_LOC_REGISTER:
            if (strcmp(loc->reg, source_reg) != 0) {
                fprintf(output, "    mov %s, %s\n", loc->reg, source_reg);
            }
            break;
        case TEMP_LOC_STACK:
            fprintf(output, "    mov [rbp - %d], %s\n", loc->offset, source_reg);
            break;
        case TEMP_LOC_NONE:
            
            codegen_set_temp_on_stack(ctx, temp_idx, ctx->temp_stack_base + temp_idx * 8);
            fprintf(output, "    mov [rbp - %d], %s\n", ctx->temp_stack_base + temp_idx * 8, source_reg);
            break;
    }
}

void codegen_emit_load_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* target_reg) {
    int offset = codegen_get_var_offset(ctx, var_name);
    fprintf(output, "    mov %s, [rbp - %d]\n", target_reg, offset);
}

void codegen_emit_store_var(FILE* output, CodegenContext* ctx, const char* var_name, const char* source_reg) {
    int offset = codegen_get_var_offset(ctx, var_name);
    fprintf(output, "    mov [rbp - %d], %s\n", offset, source_reg);
}

typedef struct {
    FILE* output;
    CodegenContext* ctx;
} Emitter;

static void emit_load_value(Emitter* em, KrtIRValue* value, const char* target_reg) {
    switch (value->type) {
        case KRT_IR_VALUE_IMM: {
            long long imm = (long long)value->data.imm;
            if (imm == 0) {
                fprintf(em->output, "    xor %s, %s\n", target_reg, target_reg);
            } else {
                fprintf(em->output, "    mov %s, %lld\n", target_reg, imm);
            }
            break;
        }
        case KRT_IR_VALUE_VAR:
            codegen_emit_load_var(em->output, em->ctx, value->data.name, target_reg);
            break;
        case KRT_IR_VALUE_TEMP:
            codegen_emit_load_temp(em->output, em->ctx, value->data.index, target_reg);
            break;
        case KRT_IR_VALUE_ARG: {
            
            int arg_idx = value->data.index;
            if (arg_idx >= 0 && arg_idx < em->ctx->current_func->param_count &&
                em->ctx->current_func->params && em->ctx->current_func->params[arg_idx].name) {
                const char* param_name = em->ctx->current_func->params[arg_idx].name;
                int offset = codegen_get_var_offset(em->ctx, param_name);
                fprintf(em->output, "    mov %s, [rbp - %d]\n", target_reg, offset);
            }
            break;
        }
        case KRT_IR_VALUE_STRING_CONST: {
            
            int str_id = value->data.string_const_id;
            fprintf(em->output, "    mov %s, string_const_%d\n", target_reg, str_id);
            break;
        }
        default:
            break;
    }
}

static void emit_store_result(Emitter* em, KrtIRValue* result, const char* source_reg) {
    switch (result->type) {
        case KRT_IR_VALUE_VAR:
            codegen_emit_store_var(em->output, em->ctx, result->data.name, source_reg);
            break;
        case KRT_IR_VALUE_TEMP:
            
            codegen_emit_store_temp(em->output, em->ctx, result->data.index, source_reg);
            break;
        default:
            break;
    }
}

static void emit_binary_op(Emitter* em, KrtIROpcode op, KrtIRValue* lhs, KrtIRValue* rhs, KrtIRValue* result) {
    
    emit_load_value(em, lhs, "rax");

    emit_load_value(em, rhs, "r8");

    switch (op) {
        case KRT_IR_ADD: fprintf(em->output, "    add rax, r8\n"); break;
        case KRT_IR_SUB: fprintf(em->output, "    sub rax, r8\n"); break;
        case KRT_IR_MUL: fprintf(em->output, "    imul rax, r8\n"); break;
        case KRT_IR_DIV:
            fprintf(em->output, "    xor rdx, rdx\n");
            fprintf(em->output, "    idiv r8\n");
            break;
        case KRT_IR_MOD:
            fprintf(em->output, "    xor rdx, rdx\n");
            fprintf(em->output, "    idiv r8\n");
            fprintf(em->output, "    mov rax, rdx\n");
            break;
        case KRT_IR_AND: fprintf(em->output, "    and rax, r8\n"); break;
        case KRT_IR_OR:  fprintf(em->output, "    or rax, r8\n"); break;
        case KRT_IR_XOR: fprintf(em->output, "    xor rax, r8\n"); break;
        default:
            break;
    }

    emit_store_result(em, result, "rax");
}

static void emit_compare(Emitter* em, KrtIROpcode op, KrtIRValue* lhs, KrtIRValue* rhs, KrtIRValue* result) {
    
    emit_load_value(em, lhs, "rax");
    emit_load_value(em, rhs, "r9");

    fprintf(em->output, "    cmp rax, r9\n");
    
    switch (op) {
        case KRT_IR_EQ: fprintf(em->output, "    sete al\n"); break;
        case KRT_IR_NE: fprintf(em->output, "    setne al\n"); break;
        case KRT_IR_LT: fprintf(em->output, "    setl al\n"); break;
        case KRT_IR_LE: fprintf(em->output, "    setle al\n"); break;
        case KRT_IR_GT: fprintf(em->output, "    setg al\n"); break;
        case KRT_IR_GE: fprintf(em->output, "    setge al\n"); break;
        default:
            break;
    }

    fprintf(em->output, "    movzx rax, al\n");
    
    emit_store_result(em, result, "rax");
}

static void emit_load(Emitter* em, KrtIRValue* var, KrtIRValue* result) {
    if (var->type != KRT_IR_VALUE_VAR) return;
    
    codegen_emit_load_var(em->output, em->ctx, var->data.name, "rax");
    emit_store_result(em, result, "rax");
}

static void emit_store(Emitter* em, KrtIRValue* var, KrtIRValue* value) {
    if (var->type != KRT_IR_VALUE_VAR) return;
    
    emit_load_value(em, value, "rax");
    codegen_emit_store_var(em->output, em->ctx, var->data.name, "rax");
}

static void emit_branch(Emitter* em, KrtIRValue* cond, const char* true_label, const char* false_label) {
    emit_load_value(em, cond, "rax");
    fprintf(em->output, "    test rax, rax\n");
    fprintf(em->output, "    jnz %s\n", true_label);
    fprintf(em->output, "    jmp %s\n", false_label);
}

static void emit_jump(Emitter* em, const char* label) {
    fprintf(em->output, "    jmp %s\n", label);
}

static void emit_call(Emitter* em, const char* func_name, KrtIRValue* args, int arg_count, KrtIRValue* result) {
    
#ifdef __linux__
    const char* arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
#else
    const char* arg_regs[] = {"rcx", "rdx", "r8", "r9"};
#endif
    int arg_reg_count = sizeof(arg_regs) / sizeof(arg_regs[0]);

    int need_save_r10_r11 = 0;
    for (int i = 0; i < arg_count && i < arg_reg_count; i++) {
        if (args[i].type == KRT_IR_VALUE_TEMP || args[i].type == KRT_IR_VALUE_VAR) {

            need_save_r10_r11 = 1;
            break;
        }
    }

    for (int i = 0; i < arg_count && i < arg_reg_count; i++) {
        emit_load_value(em, &args[i], arg_regs[i]);
    }

    if (need_save_r10_r11) {
        fprintf(em->output, "    push r10\n");
        fprintf(em->output, "    push r11\n");
    }

    const char* actual_func_name = func_name;
    if (strcmp(func_name, "print_string") == 0) {
        actual_func_name = "_print_string";
    } else if (strcmp(func_name, "println_string") == 0) {
        actual_func_name = "_println_string";
    }
    fprintf(em->output, "    call %s\n", actual_func_name);
    
    if (need_save_r10_r11) {
        fprintf(em->output, "    pop r11\n");
        fprintf(em->output, "    pop r10\n");
    }
    
    if (result && result->type != KRT_IR_VALUE_VOID) {
        emit_store_result(em, result, "rax");
    }
}

static void emit_return(Emitter* em, KrtIRValue* value) {
    if (value && value->type != KRT_IR_VALUE_VOID) {
        emit_load_value(em, value, "rax");
    }
    fprintf(em->output, "    jmp %s_epilogue\n", em->ctx->current_func->name);
}

static void emit_array_store(Emitter* em, KrtIRInst* inst) {
    if (inst->operand_count < 3) return;

    KrtIRValue* arr = &inst->operands[0];
    KrtIRValue* index = &inst->operands[1];
    KrtIRValue* value = &inst->operands[2];

    emit_load_value(em, arr, "rcx");

    emit_load_value(em, index, "rdx");

    emit_load_value(em, value, "rax");

    fprintf(em->output, "    mov [rcx + rdx*8], rax\n");
}

static void emit_instruction(Emitter* em, KrtIRInst* inst) {
    switch (inst->opcode) {
        case KRT_IR_ADD:
        case KRT_IR_SUB:
        case KRT_IR_MUL:
        case KRT_IR_DIV:
        case KRT_IR_MOD:
        case KRT_IR_AND:
        case KRT_IR_OR:
        case KRT_IR_XOR:
            if (inst->operand_count >= 2) {
                emit_binary_op(em, inst->opcode, &inst->operands[0], &inst->operands[1], &inst->result);
            }
            break;
            
        case KRT_IR_LT:
        case KRT_IR_GT:
        case KRT_IR_EQ:
        case KRT_IR_LE:
        case KRT_IR_GE:
        case KRT_IR_NE:
            if (inst->operand_count >= 2) {
                emit_compare(em, inst->opcode, &inst->operands[0], &inst->operands[1], &inst->result);
            }
            break;
            
        case KRT_IR_LOAD:
            if (inst->operand_count >= 1) {
                emit_load(em, &inst->operands[0], &inst->result);
            }
            break;
            
        case KRT_IR_STORE:
            if (inst->operand_count >= 2) {
                emit_store(em, &inst->operands[0], &inst->operands[1]);
            }
            break;
            
        case KRT_IR_BRANCH:
            if (inst->operand_count >= 3) {
                char true_label[128], false_label[128];
                snprintf(true_label, sizeof(true_label), "%s_%s", em->ctx->current_func->name, inst->operands[1].data.name);
                snprintf(false_label, sizeof(false_label), "%s_%s", em->ctx->current_func->name, inst->operands[2].data.name);
                emit_branch(em, &inst->operands[0], true_label, false_label);
            }
            break;
            
        case KRT_IR_JUMP:
            if (inst->operand_count >= 1) {
                char label[128];
                snprintf(label, sizeof(label), "%s_%s", em->ctx->current_func->name, inst->operands[0].data.name);
                emit_jump(em, label);
            }
            break;
            
        case KRT_IR_CALL:
            if (inst->operand_count >= 1) {
                const char* func_name = inst->operands[0].data.function_name;
                emit_call(em, func_name, &inst->operands[1], inst->operand_count - 1, &inst->result);
            }
            break;
            
        case KRT_IR_RETURN:
            if (inst->operand_count >= 1) {
                emit_return(em, &inst->operands[0]);
            } else {
                emit_return(em, NULL);
            }
            break;
            
        case KRT_IR_IMM:
            if (inst->operand_count >= 1) {
                emit_load_value(em, &inst->operands[0], "rax");
                emit_store_result(em, &inst->result, "rax");
            }
            break;

        case KRT_IR_ALLOC:
        case KRT_IR_NOP:

            break;
            
        case KRT_IR_ARRAY_STORE:
            emit_array_store(em, inst);
            break;
            
        case KRT_IR_LOADPTR:
            if (inst->operand_count >= 2) {
                emit_load_value(em, &inst->operands[0], "rcx");
                int offset = (int)inst->operands[1].data.imm;
                fprintf(em->output, "    mov rax, [rcx + %d]\n", offset);
                emit_store_result(em, &inst->result, "rax");
            } else if (inst->operand_count >= 1) {
                emit_load_value(em, &inst->operands[0], "rcx");
                fprintf(em->output, "    mov rax, [rcx]\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;

        case KRT_IR_STOREPTR:
            if (inst->operand_count >= 3) {
                emit_load_value(em, &inst->operands[0], "rcx");
                emit_load_value(em, &inst->operands[2], "rax");
                int offset = (int)inst->operands[1].data.imm;
                fprintf(em->output, "    mov [rcx + %d], rax\n", offset);
            }
            break;
            
        case KRT_IR_STRCAT:

            if (inst->operand_count >= 2) {
#ifdef __linux__
                emit_load_value(em, &inst->operands[0], "rdi");
                emit_load_value(em, &inst->operands[1], "rsi");
#else
                emit_load_value(em, &inst->operands[0], "rcx");
                emit_load_value(em, &inst->operands[1], "rdx");
#endif
                fprintf(em->output, "    call KrtStringConcat\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;

        case KRT_IR_INT_TO_STRING:

            if (inst->operand_count >= 1) {
#ifdef __linux__
                emit_load_value(em, &inst->operands[0], "rdi");
#else
                emit_load_value(em, &inst->operands[0], "rcx");
#endif
                fprintf(em->output, "    call KrtIntToString\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;

        case KRT_IR_DOUBLE_TO_STRING:

            if (inst->operand_count >= 1) {

                fprintf(em->output, "    call KrtDoubleToString\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_LSHIFT:
            
            if (inst->operand_count >= 2) {
                emit_load_value(em, &inst->operands[0], "rax");
                emit_load_value(em, &inst->operands[1], "rcx");
                fprintf(em->output, "    shl rax, cl\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_RSHIFT:
            
            if (inst->operand_count >= 2) {
                emit_load_value(em, &inst->operands[0], "rax");
                emit_load_value(em, &inst->operands[1], "rcx");
                fprintf(em->output, "    sar rax, cl\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_POW:
            
            if (inst->operand_count >= 2) {
                emit_load_value(em, &inst->operands[0], "rcx");  
                emit_load_value(em, &inst->operands[1], "rdx");  
                fprintf(em->output, "    call KrtPow\n");
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_CAST:
            
            if (inst->operand_count >= 1) {
                emit_load_value(em, &inst->operands[0], "rax");
                
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_COPY:
            
            if (inst->operand_count >= 1) {
                emit_load_value(em, &inst->operands[0], "rax");
                emit_store_result(em, &inst->result, "rax");
            }
            break;
            
        case KRT_IR_LABEL:
            
            break;
            
        default:
            break;
    }
}

static void emit_basic_block(Emitter* em, KrtIRBasicBlock* block) {
    if (!block) return;
    
    fprintf(em->output, "%s_%s:\n", em->ctx->current_func->name, block->label);
    
    KrtIRInst* inst = block->first_inst;
    while (inst) {
        emit_instruction(em, inst);
        inst = inst->next;
    }
}

static const char* callee_saved_regs[] = {"rbx", "r12", "r13", "r14", "r15"};
#define NUM_CALLEE_SAVED_REGS 5

static int collect_used_callee_saved_regs(CodegenContext* ctx, const char** used_regs) {
    int count = 0;
    for (int i = 0; i < 256 && count < NUM_CALLEE_SAVED_REGS; i++) {
        TempLocation* loc = &ctx->temp_locations[i];
        if (loc->type == TEMP_LOC_REGISTER) {
            
            for (int j = 0; j < NUM_CALLEE_SAVED_REGS; j++) {
                if (strcmp(loc->reg, callee_saved_regs[j]) == 0) {
                    
                    int already_added = 0;
                    for (int k = 0; k < count; k++) {
                        if (strcmp(used_regs[k], loc->reg) == 0) {
                            already_added = 1;
                            break;
                        }
                    }
                    if (!already_added) {
                        used_regs[count++] = loc->reg;
                    }
                    break;
                }
            }
        }
    }
    return count;
}

static void emit_function_prologue(FILE* output, CodegenContext* ctx) {
    
    const char* used_callee_saved[NUM_CALLEE_SAVED_REGS];
    int num_used = collect_used_callee_saved_regs(ctx, used_callee_saved);

    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");

    for (int i = 0; i < num_used; i++) {
        fprintf(output, "    push %s\n", used_callee_saved[i]);
    }

    fprintf(output, "    sub rsp, %d\n", ctx->stack_size);

#ifdef __linux__
    static const char* arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
#else
    static const char* arg_regs[] = {"rcx", "rdx", "r8", "r9"};
#endif
    int arg_reg_count = sizeof(arg_regs) / sizeof(arg_regs[0]);
    for (int i = 0; i < ctx->current_func->param_count && i < arg_reg_count; i++) {

        if (ctx->current_func->params && ctx->current_func->params[i].name) {
            const char* param_name = ctx->current_func->params[i].name;
            int offset = codegen_get_var_offset(ctx, param_name);
            fprintf(output, "    mov [rbp - %d], %s\n", offset, arg_regs[i]);
        }
    }
}

static void emit_function_epilogue(FILE* output, CodegenContext* ctx) {
    
    const char* used_callee_saved[NUM_CALLEE_SAVED_REGS];
    int num_used = collect_used_callee_saved_regs(ctx, used_callee_saved);

    fprintf(output, "%s_epilogue:\n", ctx->current_func->name);
    fprintf(output, "    mov rsp, rbp\n");

    for (int i = num_used - 1; i >= 0; i--) {
        fprintf(output, "    pop %s\n", used_callee_saved[i]);
    }

    fprintf(output, "    pop rbp\n");
    fprintf(output, "    ret\n");
}

static void emit_function(FILE* output, KrtIRFunction* func, KrtIRModule* module) {
    if (!func || !func->name) {
        return;
    }

    CodegenContext ctx;
    codegen_context_init(&ctx, func, module);

    Emitter em = {output, &ctx};

    fprintf(output, "\nglobal %s\n", func->name);
    fprintf(output, "; Function: %s\n", func->name);
    fprintf(output, "%s:\n", func->name);

    emit_function_prologue(output, &ctx);

    KrtIRBasicBlock* block = func->entry_block;
    int block_count = 0;
    while (block) {
        emit_basic_block(&em, block);
        block = block->next;
        block_count++;
    }

    emit_function_epilogue(output, &ctx);
}

void KrtX86Generate(FILE* output, KrtIRModule* module) {
    if (!output || !module) {
        return;
    }
    
    fprintf(output, "section .data\n\n");
    fprintf(output, "section .rodata\n");

    for (int i = 0; i < module->string_const_count && module->string_constants; i++) {
        const char* str = module->string_constants[i];
        if (!str) continue;
        fprintf(output, "string_const_%d: db ", i);

        int need_comma = 0;
        for (const char* p = str; *p; p++) {
            if (need_comma) {
                fprintf(output, ", ");
            }
            need_comma = 1;
            
            switch (*p) {
                case '\n': fprintf(output, "10"); break;  
                case '\r': fprintf(output, "13"); break;  
                case '\t': fprintf(output, "9"); break;   
                case '\\': fprintf(output, "'\\\\'"); break;
                case '"': fprintf(output, "'\\\"'"); break;
                case '\0': fprintf(output, "0"); break;
                default:
                    if (*p >= 32 && *p < 127) {
                        fprintf(output, "'%c'", *p);
                    } else {
                        
                        fprintf(output, "%d", (unsigned char)*p);
                    }
            }
        }
        fprintf(output, ", 0\n");
    }

    if (module->string_const_count == 0) {
        fprintf(output, "string_const_empty: db 0\n");
    }
    fprintf(output, "\n");

    fprintf(output, "section .text\n\n");

    fprintf(output, "extern exit\n");
    fprintf(output, "extern _print_int\n");
    fprintf(output, "extern _print_int64\n");
    fprintf(output, "extern _print_float\n");
    fprintf(output, "extern _print_string\n");
    fprintf(output, "extern _println_string\n");
    fprintf(output, "extern Console__WriteLine\n");
    fprintf(output, "extern Console__Write\n");
    fprintf(output, "extern Console__WriteLineInt\n");
    fprintf(output, "extern Console__WriteInt\n");
    fprintf(output, "extern KrtMalloc\n");
    fprintf(output, "extern KrtFree\n");
    fprintf(output, "extern KrtRealloc\n");
    fprintf(output, "extern KrtStrcat\n");
    fprintf(output, "extern KrtIntToString\n");
    fprintf(output, "extern KrtDoubleToString\n");
    fprintf(output, "extern KrtPow\n");
    fprintf(output, "extern timer_start\n");
    fprintf(output, "extern timer_start_int\n");
    fprintf(output, "extern timer_elapsed\n");
    fprintf(output, "extern timer_elapsed_int\n");
    fprintf(output, "extern timer_current\n");
    fprintf(output, "extern timer_current_int\n");
    fprintf(output, "extern KrtIsInstance\n");
    fprintf(output, "extern KrtAsInstance\n");
    fprintf(output, "extern KrtNullCoalesce\n");
    fprintf(output, "extern KrtNullConditional\n");
    fprintf(output, "extern KrtCastToInt32\n");
    fprintf(output, "extern KrtCastToInt64\n");
    fprintf(output, "extern KrtCastToFloat32\n");
    fprintf(output, "extern KrtCastToFloat64\n");
    fprintf(output, "extern KrtCastToBool\n");
    fprintf(output, "extern KrtCastToString\n");
    fprintf(output, "extern KrtSizeOfInt32\n");
    fprintf(output, "extern KrtSizeOfInt64\n");
    fprintf(output, "extern KrtSizeOfFloat32\n");
    fprintf(output, "extern KrtSizeOfFloat64\n");
    fprintf(output, "extern KrtSizeOfPointer\n");
    fprintf(output, "extern KrtStringConcat\n");
    fprintf(output, "extern KrtInt32ToString\n");
    fprintf(output, "extern KrtInt64ToString\n");
    fprintf(output, "extern KrtFloat32ToString\n");
    fprintf(output, "extern KrtFloat64ToString\n");
    fprintf(output, "extern KrtBoolToString\n");
    fprintf(output, "extern KrtCreateTuple\n");
    fprintf(output, "extern KrtTupleSetElement\n");
    fprintf(output, "extern KrtTupleGetElement\n");
    fprintf(output, "extern KrtLoadPtr\n");
    fprintf(output, "extern KrtStorePtr\n");
    fprintf(output, "extern KrtStackAlloc\n");
    fprintf(output, "extern KrtPinObject\n");
    fprintf(output, "extern KrtUnpinObject\n");
    fprintf(output, "extern KrtAwaitTask\n");
    fprintf(output, "extern KrtCreateTask\n");
    fprintf(output, "extern KrtCompleteTask\n");
    fprintf(output, "extern KrtLinqWhere\n");
    fprintf(output, "extern KrtLinqOrderBy\n");
    fprintf(output, "extern KrtLinqGroupBy\n");
    fprintf(output, "extern KrtLinqSelect\n");
    fprintf(output, "extern KrtCreateDelegate\n");
    fprintf(output, "extern KrtInvokeDelegate\n");
    fprintf(output, "extern Monitor_Enter\n");
    fprintf(output, "extern Monitor_Exit\n");
    fprintf(output, "extern KrtThrowException\n");
    fprintf(output, "extern KrtRethrowException\n");
    fprintf(output, "extern KrtGetGenericStaticField\n");
    fprintf(output, "extern KrtSetGenericStaticField\n");
    fprintf(output, "extern KrtClearGenericStaticFields\n");
    fprintf(output, "extern KrtIsClass\n");
    fprintf(output, "extern KrtIsStruct\n");
    fprintf(output, "extern KrtImplementsInterface\n");
    fprintf(output, "extern KrtInheritsFrom\n");
    fprintf(output, "extern KrtCheckGenericConstraint\n\n");

    KrtIRFunction* func = module->functions;
    int func_count = 0;
    while (func) {
        emit_function(output, func, module);
        func = func->next;
        func_count++;
    }
}