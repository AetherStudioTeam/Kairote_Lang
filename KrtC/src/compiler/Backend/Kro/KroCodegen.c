#include "KroCodegen.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#define X86_MOV_R64_IMM64   0x48B8  
#define X86_MOV_R64_R64     0x4889  
#define X86_PUSH_R64        0x50    
#define X86_POP_R64         0x58    
#define X86_RET             0xC3    
#define X86_CALL_REL32      0xE8    
#define X86_JMP_REL32       0xE9    
#define X86_ADD_R64_IMM32   0x4881  
#define X86_SUB_R64_IMM32   0x4881  
#define X86_XOR_R64_R64     0x4831  

#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7
#define REG_R8  8
#define REG_R9  9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15

static void emit_byte(KROCodegenContext* ctx, uint8_t byte) {
    uint8_t data = byte;
    kro_write_code(ctx->writer, &data, 1);
}

static void emit_bytes(KROCodegenContext* ctx, const uint8_t* data, uint32_t size) {
    kro_write_code(ctx->writer, data, size);
}

static void emit_u32(KROCodegenContext* ctx, uint32_t value) {
    kro_write_code(ctx->writer, &value, 4);
}

static void emit_u64(KROCodegenContext* ctx, uint64_t value) {
    kro_write_code(ctx->writer, &value, 8);
}

static void emit_load_imm64_to_reg(KROCodegenContext* ctx, uint64_t value, int reg) {
    if (reg < 8) {
        
        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0xB8 + reg);
        emit_u64(ctx, value);
    } else {
        
        emit_byte(ctx, 0x49);
        emit_byte(ctx, 0xB8 + (reg - 8));
        emit_u64(ctx, value);
    }
}

static void emit_load_string_addr_to_reg(KROCodegenContext* ctx, int32_t sym_idx, int reg) {
    
    if (reg < 8) {
        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0xB8 + reg);
    } else {
        emit_byte(ctx, 0x49);
        emit_byte(ctx, 0xB8 + (reg - 8));
    }
    
    uint32_t offset = kro_get_code_offset(ctx->writer);
    emit_u64(ctx, 0); 
    
    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, offset, sym_idx, KRO_RELOC_ABS64, 0);
}

static void emit_function_prologue(KROCodegenContext* ctx, int stack_size) {
    
    emit_byte(ctx, 0x55);
    
    emit_bytes(ctx, (const uint8_t*)"\x48\x89\xE5", 3);
    
    if (stack_size > 0) {
        emit_bytes(ctx, (const uint8_t*)"\x48\x81\xEC", 3);
        emit_u32(ctx, (uint32_t)stack_size);
    }
}

static void emit_function_epilogue(KROCodegenContext* ctx) {
    
    emit_bytes(ctx, (const uint8_t*)"\x48\x89\xEC", 3);
    
    emit_byte(ctx, 0x5D);
    
    emit_byte(ctx, X86_RET);
}

static void emit_load_imm64(KROCodegenContext* ctx, uint64_t value) {
    
    emit_byte(ctx, 0x48);
    emit_byte(ctx, 0xB8);
    emit_u64(ctx, value);
}

static void emit_call_external(KROCodegenContext* ctx, const char* func_name) {

    int32_t sym_idx = kro_find_symbol(ctx->writer, func_name);
    if (sym_idx < 0) {
        
        if (strcmp(func_name, "Console__Write") == 0 ||
            strcmp(func_name, "Console__WriteLine") == 0) {
            
            sym_idx = kro_add_import_symbol(ctx->writer, "WriteConsoleA", "kernel32.dll");
        } else if (strcmp(func_name, "Console__ReadLine") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "ReadConsoleA", "kernel32.dll");
        } else {
            
            sym_idx = kro_add_undefined_symbol(ctx->writer, func_name);
        }
    }

    uint32_t call_offset = kro_get_code_offset(ctx->writer);
    
    emit_byte(ctx, 0xE8);
    emit_u32(ctx, 0);

    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, call_offset + 1, sym_idx, KRO_RELOC_PC32, 0);
}

static void emit_call_local(KROCodegenContext* ctx, const char* func_name) {
    int32_t sym_idx = kro_find_symbol(ctx->writer, func_name);
    if (sym_idx < 0) {
        
        sym_idx = kro_add_undefined_symbol(ctx->writer, func_name);
    }

    uint32_t call_offset = kro_get_code_offset(ctx->writer);

    emit_byte(ctx, 0xE8);
    emit_u32(ctx, 0);

    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, call_offset + 1, sym_idx, KRO_RELOC_PC32, 0);
}

static void emit_return(KROCodegenContext* ctx) {
    emit_function_epilogue(ctx);
}

static void emit_binary_op(KROCodegenContext* ctx, int op) {
    
    switch (op) {
        case 0: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x01\xD8", 3);
            break;
        case 1: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x29\xD8", 3);
            break;
        case 2: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x0F\xAF\xC3", 4);
            break;
        case 3: 
            
            emit_bytes(ctx, (const uint8_t*)"\x48\x99", 2);  
            emit_bytes(ctx, (const uint8_t*)"\x48\xF7\xFB", 3);  
            break;
    }
}

static void KroGenerateInstruction(KROCodegenContext* ctx, KrtIRInst* inst) {
    if (!inst) return;

    switch (inst->opcode) {
        case KRT_IR_IMM: {
            
            if (inst->operand_count >= 1 && inst->operands[0].type == KRT_IR_VALUE_IMM) {
                uint64_t value = (uint64_t)inst->operands[0].data.imm;
                emit_load_imm64(ctx, value);
            }
            break;
        }

        case KRT_IR_ADD:
        case KRT_IR_SUB:
        case KRT_IR_MUL:
        case KRT_IR_DIV: {
            
            if (inst->operand_count >= 2) {
                KrtIRValue* lhs = &inst->operands[0];
                KrtIRValue* rhs = &inst->operands[1];
                
                if (lhs->type == KRT_IR_VALUE_ARG) {
                    
                    int arg_idx = lhs->data.index;
                    if (arg_idx == 0) {
                        
                        emit_bytes(ctx, (const uint8_t*)"\x48\x89\xC8", 3);
                    } else if (arg_idx == 1) {
                        
                        emit_bytes(ctx, (const uint8_t*)"\x48\x89\xD0", 3);
                    }
                } else if (lhs->type == KRT_IR_VALUE_IMM) {
                    emit_load_imm64(ctx, (uint64_t)lhs->data.imm);
                }
                
                if (rhs->type == KRT_IR_VALUE_ARG) {
                    int arg_idx = rhs->data.index;
                    if (inst->opcode == KRT_IR_ADD) {
                        if (arg_idx == 0) {
                            
                            emit_bytes(ctx, (const uint8_t*)"\x48\x01\xC8", 3);
                        } else if (arg_idx == 1) {
                            
                            emit_bytes(ctx, (const uint8_t*)"\x48\x01\xD0", 3);
                        }
                    } else if (inst->opcode == KRT_IR_SUB) {
                        if (arg_idx == 0) {
                            
                            emit_bytes(ctx, (const uint8_t*)"\x48\x29\xC8", 3);
                        } else if (arg_idx == 1) {
                            
                            emit_bytes(ctx, (const uint8_t*)"\x48\x29\xD0", 3);
                        }
                    }
                    
                } else if (rhs->type == KRT_IR_VALUE_IMM) {
                    
                    int op = inst->opcode - KRT_IR_ADD;
                    emit_binary_op(ctx, op);
                }
            }
            break;
        }

        case KRT_IR_CALL: {
            
            if (inst->operand_count >= 1 && inst->operands[0].type == KRT_IR_VALUE_FUNCTION) {
                const char* func_name = inst->operands[0].data.function_name;
                
                static const int arg_regs[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
                int arg_count = inst->operand_count - 1;
                if (arg_count > 4) arg_count = 4; 
                
                for (int i = 0; i < arg_count; i++) {
                    KrtIRValue* arg = &inst->operands[i + 1];
                    int target_reg = arg_regs[i];
                    
                    if (arg->type == KRT_IR_VALUE_IMM) {
                        emit_load_imm64_to_reg(ctx, (uint64_t)arg->data.imm, target_reg);
                    } else if (arg->type == KRT_IR_VALUE_STRING_CONST) {
                        int32_t sym_idx = -1;
                        if (arg->data.string_const_id >= 0 && ctx->string_const_sym_indices) {
                            sym_idx = ctx->string_const_sym_indices[arg->data.string_const_id];
                        }
                        
                        if (sym_idx < 0) {
                            sym_idx = kro_find_symbol(ctx->writer, "empty_str");
                            if (sym_idx < 0) {
                                uint32_t off = kro_get_rodata_offset(ctx->writer);
                                kro_write_rodata(ctx->writer, "\0", 1);
                                sym_idx = kro_add_symbol(ctx->writer, "empty_str", KRO_SYM_OBJECT, KRO_BIND_LOCAL, KRO_SEC_RODATA, off);
                            }
                        }
                        if (sym_idx >= 0) {
                            emit_load_string_addr_to_reg(ctx, sym_idx, target_reg);
                        }
                    }
                    
                }
                
                int32_t local_sym_idx = kro_find_symbol(ctx->writer, func_name);
                if (local_sym_idx >= 0) {
                    
                    emit_call_local(ctx, func_name);
                } else {
                    
                    emit_call_external(ctx, func_name);
                }
            }
            break;
        }

        case KRT_IR_RETURN: {

            if (inst->operand_count >= 1) {

                KrtIRValue* ret_val = &inst->operands[0];
                if (ret_val->type == KRT_IR_VALUE_IMM) {

                    uint64_t value = (uint64_t)ret_val->data.imm;
                    emit_load_imm64(ctx, value);
                }

            }
            
            break;
        }

        case KRT_IR_STORE: {
            if (inst->operand_count >= 2) {
                KrtIRValue* dest = &inst->operands[0];
                KrtIRValue* value = &inst->operands[1];
                
                if (dest->type == KRT_IR_VALUE_VAR) {
                    int sym_idx = kro_find_symbol(ctx->writer, dest->data.name);
                    if (sym_idx < 0) {
                        break;
                    }
                    
                    if (value->type == KRT_IR_VALUE_IMM) {
                        emit_load_imm64_to_reg(ctx, (uint64_t)value->data.imm, REG_RAX);
                    }
                    
                    emit_byte(ctx, 0x48);
                    emit_byte(ctx, 0xA3);
                    uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                    emit_u32(ctx, 0);
                    
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                }
            }
            break;
        }

        case KRT_IR_LOAD: {
            if (inst->operand_count >= 1) {
                KrtIRValue* src = &inst->operands[0];
                
                if (src->type == KRT_IR_VALUE_VAR) {
                    int sym_idx = kro_find_symbol(ctx->writer, src->data.name);
                    if (sym_idx < 0) {
                        break;
                    }
                    
                    emit_byte(ctx, 0x48);
                    emit_byte(ctx, 0x8B);
                    emit_byte(ctx, 0x05);
                    uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                    emit_u32(ctx, 0);
                    
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                }
            }
            break;
        }

        default:
            
            break;
    }
}

static void KroGenerateBlock(KROCodegenContext* ctx, KrtIRBasicBlock* block) {
    if (!block) return;

    KrtIRInst* inst = block->first_inst;
    while (inst) {
        KroGenerateInstruction(ctx, inst);
        inst = inst->next;
    }
}

static int calculate_function_stack_size(KrtIRFunction* func) {
    int stack_size = 32;  

    stack_size += 8;

    stack_size = (stack_size + 15) & ~15;  
    if (stack_size % 16 == 0) {
        stack_size += 8;  
    }
    if (stack_size < 48) stack_size = 48;

    return stack_size;
}

static void emit_call_exit_process(KROCodegenContext* ctx) {
    
    emit_bytes(ctx, (const uint8_t*)"\x48\x89\xC1", 3);  

    int32_t sym_idx = kro_find_symbol(ctx->writer, "ExitProcess");
    if (sym_idx < 0) {
        sym_idx = kro_add_import_symbol(ctx->writer, "ExitProcess", "kernel32.dll");
    }

    uint32_t mov_offset = kro_get_code_offset(ctx->writer);

    emit_bytes(ctx, (const uint8_t*)"\x48\x8B\x05", 3);  
    emit_u32(ctx, 0);
    
    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, mov_offset + 3, sym_idx, KRO_RELOC_PC32, 0);
    
    emit_bytes(ctx, (const uint8_t*)"\xFF\xD0", 2);  
}

static void KroGenerateFunction(KROCodegenContext* ctx, KrtIRFunction* func, KrtIRModule* module) {
    if (!func) return;

    uint32_t func_offset = kro_get_code_offset(ctx->writer);

    int is_main = (strcmp(func->name, "main") == 0);
    int is_mangled_main = (strcmp(func->name, "_ZN4mainEv") == 0);
    int is_entry_point = is_main || is_mangled_main;

    int stack_size = calculate_function_stack_size(func);

    emit_function_prologue(ctx, stack_size);

    bool has_explicit_return = false;
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {

        KrtIRInst* inst = block->first_inst;
        while (inst) {
            if (inst->opcode == KRT_IR_RETURN) {
                has_explicit_return = true;
            }
            inst = inst->next;
        }
        KroGenerateBlock(ctx, block);
        block = block->next;
    }

    if (is_entry_point) {
        
        int called_mangled_main = 0;
        if (is_main && !has_explicit_return && module) {
            
            KrtIRFunction* mangled_main = module->functions;
            while (mangled_main) {
                if (strcmp(mangled_main->name, "_ZN4mainEv") == 0) {
                    
                    emit_call_local(ctx, "_ZN4mainEv");
                    called_mangled_main = 1;
                    break;
                }
                mangled_main = mangled_main->next;
            }
        }
        
        if (!has_explicit_return && !called_mangled_main) {
            emit_load_imm64(ctx, 0);
        }
        
        emit_function_epilogue(ctx);
    } else {
        if (!has_explicit_return) {
            emit_load_imm64(ctx, 0);
        }
        emit_function_epilogue(ctx);
    }
}

static void KroGenerateDataSection(KROCodegenContext* ctx, KrtIRModule* module) {
    if (!module) return;

    if (module->string_const_count > 0 && module->string_constants) {
        ctx->string_const_sym_indices = (int32_t*)malloc(module->string_const_count * sizeof(int32_t));
        for (int i = 0; i < module->string_const_count; i++) {
            const char* str = module->string_constants[i];
            if (!str) continue;
            uint32_t rodata_offset = kro_get_rodata_offset(ctx->writer);
            
            kro_write_rodata(ctx->writer, str, (uint32_t)strlen(str) + 1);
            
            char sym_name[32];
            snprintf(sym_name, sizeof(sym_name), "str_const_%d", i);
            ctx->string_const_sym_indices[i] = kro_add_symbol(ctx->writer, sym_name, KRO_SYM_OBJECT, KRO_BIND_LOCAL, KRO_SEC_RODATA, rodata_offset);
        }
    }

    for (int i = 0; i < module->global_count; i++) {
        KrtIRGlobal* global = &module->globals[i];
        if (!global || !global->name) continue;
        
        uint32_t data_offset = kro_get_data_offset(ctx->writer);
        
        int64_t init_value = 0;
        if (global->has_initializer) {
            init_value = (int64_t)global->init_number;
        }
        
        kro_write_data(ctx->writer, &init_value, 8);
        
        int sym_idx = kro_add_symbol(ctx->writer, global->name, KRO_SYM_OBJECT, KRO_BIND_GLOBAL, KRO_SEC_DATA, data_offset);
        if (sym_idx < 0) {
        }
    }

}

void KrtKrtGenerate(FILE* output_file, const char* output_filename, KrtIRModule* module) {
    if (!output_file || !module) {
        return;
    }

    KROCodegenContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.output_file = output_file;
    if (output_filename) {
        KRT_STRNCPY(ctx.output_filename, output_filename, sizeof(ctx.output_filename));
    } else {
        strcpy(ctx.output_filename, "output.kro");
    }
    ctx.writer = kro_writer_create();
    ctx.temp_var_count = 0;
    ctx.label_count = 0;

    if (!ctx.writer) {
        return;
    }

    KroGenerateDataSection(&ctx, module);

    int func_count = 0;
    KrtIRFunction* temp_func = module->functions;
    while (temp_func) {
        func_count++;
        temp_func = temp_func->next;
    }

    int* sym_indices = (int*)malloc(func_count * sizeof(int));
    if (!sym_indices) {
        kro_writer_destroy(ctx.writer);
        return;
    }

    int func_idx = 0;
    KrtIRFunction* func = module->functions;
    while (func) {
        uint32_t func_offset = kro_get_code_offset(ctx.writer);
        int sym_idx = kro_add_symbol(ctx.writer, func->name, KRO_SYM_FUNC, KRO_BIND_GLOBAL, KRO_SEC_TEXT, func_offset);
        sym_indices[func_idx] = sym_idx;

        int is_main = (strcmp(func->name, "main") == 0);
        int is_mangled_main = (strcmp(func->name, "_ZN4mainEv") == 0);
        if (is_main || is_mangled_main) {
            kro_set_entry_point(ctx.writer, func_offset);
        }

        func = func->next;
        func_idx++;
    }

    kro_set_code_offset(ctx.writer, 0);
    func_idx = 0;
    func = module->functions;
    while (func) {
        uint32_t actual_offset = kro_get_code_offset(ctx.writer);
        
        kro_update_symbol_value(ctx.writer, sym_indices[func_idx], actual_offset);

        int is_main = (strcmp(func->name, "main") == 0);
        int is_mangled_main = (strcmp(func->name, "_ZN4mainEv") == 0);
        if (is_main || is_mangled_main) {
            kro_set_entry_point(ctx.writer, actual_offset);
        }

        KroGenerateFunction(&ctx, func, module);
        func = func->next;
        func_idx++;
    }

    free(sym_indices);

    if (!kro_write_file(ctx.writer, ctx.output_filename)) {
    }

    if (ctx.string_const_sym_indices) {
        free(ctx.string_const_sym_indices);
    }
    kro_writer_destroy(ctx.writer);
}