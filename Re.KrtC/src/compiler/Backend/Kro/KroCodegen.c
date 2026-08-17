#include "KroCodegen.h"
#include "../../Middle/Ir/IrSsa.h"
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
#define KRO_MAX_LABEL_LEN   256
#define KRO_MIN_STACK_SIZE  48

#ifndef KRO_DEBUG
#define KRO_DEBUG 0
#endif

#if KRO_DEBUG
#define KRO_DBG(fmt, ...) fprintf(stderr, "[KroCodegen] " fmt "\n", ##__VA_ARGS__)
#else
#define KRO_DBG(fmt, ...) ((void)0)
#endif

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

#ifdef __linux__
static const int g_arg_regs[] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
static const int g_arg_reg_count = 6;
#define KRO_SHADOW_SPACE_SIZE 0
#else
static const int g_arg_regs[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
static const int g_arg_reg_count = 4;
#define KRO_SHADOW_SPACE_SIZE 32
#endif

static bool kro_module_has_mangled_main(KrtIRModule* module) {
    if (!module) return false;
    KrtIRFunction* f = module->functions;
    while (f) {
        if (f->name && strcmp(f->name, "_KrtMainEntry") == 0) return true;
        f = f->next;
    }
    return false;
}

static bool is_entry_point_function(const char* name, bool has_mangled_main) {
    if (!name) return false;
    if (strcmp(name, "_KrtMainEntry") == 0) return true;
    if (strcmp(name, "main") == 0 && !has_mangled_main) return true;
    return false;
}

static void emit_byte(KROCodegenContext* ctx, uint8_t byte);
static void emit_u32(KROCodegenContext* ctx, uint32_t value);

static KROLocalVar* find_or_alloc_local_var(KROCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;

    for (int i = 0; i < KRO_MAX_LOCAL_VARS && i < ctx->local_var_count; i++) {
        if (strcmp(ctx->local_vars[i].name, name) == 0) {
            return &ctx->local_vars[i];
        }
    }

    if (ctx->local_var_count >= KRO_MAX_LOCAL_VARS) return NULL;

    KROLocalVar* var = &ctx->local_vars[ctx->local_var_count];
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->name[sizeof(var->name) - 1] = '\0';
    var->stack_offset = ctx->current_stack_offset;
    var->allocated = 1;
    ctx->current_stack_offset += 8;
    ctx->local_var_count++;

    return var;
}

static KROLocalVar* find_local_var(KROCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;

    for (int i = 0; i < KRO_MAX_LOCAL_VARS && i < ctx->local_var_count; i++) {
        if (strcmp(ctx->local_vars[i].name, name) == 0) {
            return &ctx->local_vars[i];
        }
    }
    return NULL;
}

static void emit_store_to_stack(KROCodegenContext* ctx, int offset, int src_reg) {
    emit_byte(ctx, 0x48);
    emit_byte(ctx, 0x89);
    emit_byte(ctx, 0x85 | (src_reg << 3));
    emit_u32(ctx, (uint32_t)(-offset));
}

static void emit_load_from_stack(KROCodegenContext* ctx, int offset, int dst_reg) {
    emit_byte(ctx, 0x48);
    emit_byte(ctx, 0x8B);
    emit_byte(ctx, 0x85 | (dst_reg << 3));
    emit_u32(ctx, (uint32_t)(-offset));
}

static void emit_byte(KROCodegenContext* ctx, uint8_t byte) {
    uint8_t data = byte;
    uint32_t before = kro_get_code_offset(ctx->writer);
    kro_write_code(ctx->writer, &data, 1);
    if (kro_get_code_offset(ctx->writer) == before) {
        fprintf(stderr, "[emit_byte] WARNING: offset not updated! byte=0x%02x\n", byte);
    }
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

    if (ctx && ctx->is_main_func) {
#ifdef _WIN32
        emit_bytes(ctx, (const uint8_t*)"\x48\x89\xC1", 3);
        emit_call_external(ctx, "ExitProcess");
#else
        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0x89);
        emit_byte(ctx, 0xC7);

        emit_byte(ctx, 0x48);
        emit_byte(ctx, 0xC7);
        emit_byte(ctx, 0xC0);
        emit_u32(ctx, 60);

        emit_byte(ctx, 0x0F);
        emit_byte(ctx, 0x05);
#endif
    } else {
        emit_byte(ctx, X86_RET);
    }
}

static void emit_load_imm64(KROCodegenContext* ctx, uint64_t value) {
    emit_byte(ctx, 0x48);
    emit_byte(ctx, 0xB8);
    emit_u64(ctx, value);
}

static void emit_call_external(KROCodegenContext* ctx, const char* func_name) {
    if (!ctx || !func_name) return;

    int32_t sym_idx = kro_find_symbol(ctx->writer, func_name);
    if (sym_idx < 0) {
#ifdef _WIN32
        if (strcmp(func_name, "puts") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "puts", "msvcrt.dll");
        } else if (strcmp(func_name, "Console__Write") == 0 ||
                   strcmp(func_name, "Console__WriteLine") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "printf", "msvcrt.dll");
        } else if (strcmp(func_name, "Console__ReadLine") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "fgets", "msvcrt.dll");
        } else if (strcmp(func_name, "malloc") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "malloc", "msvcrt.dll");
        } else if (strcmp(func_name, "free") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "free", "msvcrt.dll");
        } else if (strcmp(func_name, "memcpy") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "memcpy", "msvcrt.dll");
        } else if (strcmp(func_name, "ExitProcess") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "ExitProcess", "kernel32.dll");
        } else {
            sym_idx = kro_add_undefined_symbol(ctx->writer, func_name);
        }
#else
        if (strcmp(func_name, "puts") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "puts", "libc.so.6");
        } else if (strcmp(func_name, "Console__Write") == 0 ||
                   strcmp(func_name, "Console__WriteLine") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "printf", "libc.so.6");
        } else if (strcmp(func_name, "Console__ReadLine") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "fgets", "libc.so.6");
        } else if (strcmp(func_name, "malloc") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "malloc", "libc.so.6");
        } else if (strcmp(func_name, "free") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "free", "libc.so.6");
        } else if (strcmp(func_name, "memcpy") == 0) {
            sym_idx = kro_add_import_symbol(ctx->writer, "memcpy", "libc.so.6");
        } else {
            sym_idx = kro_add_undefined_symbol(ctx->writer, func_name);
#if KRO_DEBUG
            fprintf(stderr, "[KroCodegen] External call: %s, sym_idx=%d\n", func_name, sym_idx);
#endif
        }
#endif
    }

    if (sym_idx < 0) return;

    uint32_t call_offset = kro_get_code_offset(ctx->writer);
    emit_byte(ctx, 0xE8);
    emit_u32(ctx, 0);
    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, call_offset + 1, sym_idx, KRO_RELOC_PC32, -4);
}

static void emit_call_local(KROCodegenContext* ctx, const char* func_name) {
    if (!ctx || !func_name) return;

    int32_t sym_idx = kro_find_symbol(ctx->writer, func_name);

    if (sym_idx < 0) {
        sym_idx = kro_add_undefined_symbol(ctx->writer, func_name);
    }

    if (sym_idx < 0) return;

    uint32_t call_offset = kro_get_code_offset(ctx->writer);

    emit_byte(ctx, 0xE8);
    emit_u32(ctx, 0);

    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, call_offset + 1, sym_idx, KRO_RELOC_PC32, 0);
}

static void emit_binary_op(KROCodegenContext* ctx, int op) {
    if (!ctx) return;

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
        case 4:
            emit_bytes(ctx, (const uint8_t*)"\x48\x99", 2);
            emit_bytes(ctx, (const uint8_t*)"\x48\xF7\xFB", 3);
            emit_bytes(ctx, (const uint8_t*)"\x48\x89\xD0", 3);
            break;
        default:
            break;
    }
}

static int find_temp_slot(KROCodegenContext* ctx, int temp_index) {
    if (!ctx) return -1;
    for (int i = 0; i < ctx->temp_slot_count; i++) {
        if (ctx->temp_slots[i].valid && ctx->temp_slots[i].temp_index == temp_index) {
            return ctx->temp_slots[i].stack_offset;
        }
    }
    return -1;
}

static int alloc_temp_slot(KROCodegenContext* ctx, int temp_index) {
    if (!ctx || ctx->temp_slot_count >= KRO_MAX_TEMP_REGS) return -1;
    for (int i = 0; i < ctx->temp_slot_count; i++) {
        if (ctx->temp_slots[i].valid && ctx->temp_slots[i].temp_index == temp_index) {
            return ctx->temp_slots[i].stack_offset;
        }
    }
    ctx->current_stack_offset += 8;
    ctx->temp_slots[ctx->temp_slot_count].temp_index = temp_index;
    ctx->temp_slots[ctx->temp_slot_count].stack_offset = ctx->current_stack_offset;
    ctx->temp_slots[ctx->temp_slot_count].valid = 1;
    ctx->temp_slot_count++;
    return ctx->current_stack_offset;
}

static void emit_load_value_to_reg(KROCodegenContext* ctx, KrtIRValue* value, int target_reg) {
    if (!ctx || !value) return;

    switch (value->type) {
        case KRT_IR_VALUE_IMM:
            emit_load_imm64_to_reg(ctx, (uint64_t)value->data.imm, target_reg);
            break;
        case KRT_IR_VALUE_VAR: {
            KROLocalVar* local_var = find_local_var(ctx, value->data.name);
            if (!local_var) {
                local_var = find_or_alloc_local_var(ctx, value->data.name);
            }
            if (local_var && local_var->allocated) {
                emit_load_from_stack(ctx, local_var->stack_offset, target_reg);
            } else {
                int sym_idx = kro_find_symbol(ctx->writer, value->data.name);
                if (sym_idx >= 0) {
                    emit_byte(ctx, 0x48);
                    emit_byte(ctx, 0x8B);
                    emit_byte(ctx, 0x05 + (target_reg << 3));
                    uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                    emit_u32(ctx, 0);
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                }
            }
            break;
        }
        case KRT_IR_VALUE_TEMP: {
            int stack_offset = find_temp_slot(ctx, value->data.index);
            if (stack_offset > 0) {
                emit_load_from_stack(ctx, stack_offset, target_reg);
            }
            break;
        }
        case KRT_IR_VALUE_ARG: {
            int arg_idx = value->data.index;
            if (arg_idx >= 0 && arg_idx < g_arg_reg_count) {
                int src_reg = g_arg_regs[arg_idx];
                if (src_reg < 8) {
                    emit_byte(ctx, 0x48);
                    emit_byte(ctx, 0x89);
                    emit_byte(ctx, 0xC0 | ((src_reg & 0x7) << 3) | (target_reg & 0x7));
                } else {
                    emit_byte(ctx, 0x4C);
                    emit_byte(ctx, 0x89);
                    emit_byte(ctx, 0xC0 | (((src_reg - 8) & 0x7) << 3) | (target_reg & 0x7));
                }
            }
            break;
        }
        case KRT_IR_VALUE_STRING_CONST: {
            int32_t sym_idx = -1;
            if (value->data.string_const_id >= 0 &&
                ctx->string_const_sym_indices &&
                value->data.string_const_id < ctx->string_const_count) {
                sym_idx = ctx->string_const_sym_indices[value->data.string_const_id];
            }
            if (sym_idx >= 0) {
                emit_load_string_addr_to_reg(ctx, sym_idx, target_reg);
            }
            break;
        }
        default:
            break;
    }
}

static void KroGenerateInstruction(KROCodegenContext* ctx, KrtIRInst* inst) {
    if (!ctx || !inst) return;

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
        case KRT_IR_DIV:
        case KRT_IR_MOD: {
            if (inst->operand_count < 2) break;

            KrtIRValue* lhs = &inst->operands[0];
            KrtIRValue* rhs = &inst->operands[1];

            emit_load_value_to_reg(ctx, lhs, REG_RAX);
            emit_load_value_to_reg(ctx, rhs, REG_RBX);

            int op = inst->opcode - KRT_IR_ADD;
            emit_binary_op(ctx, op);
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_AND:
        case KRT_IR_OR:
        case KRT_IR_XOR: {
            if (inst->operand_count < 2) break;

            KrtIRValue* lhs = &inst->operands[0];
            KrtIRValue* rhs = &inst->operands[1];

            emit_load_value_to_reg(ctx, lhs, REG_RAX);
            emit_load_value_to_reg(ctx, rhs, REG_RBX);

            switch (inst->opcode) {
                case KRT_IR_AND:
                    emit_bytes(ctx, (const uint8_t*)"\x48\x21\xD8", 3);
                    break;
                case KRT_IR_OR:
                    emit_bytes(ctx, (const uint8_t*)"\x48\x09\xD8", 3);
                    break;
                case KRT_IR_XOR:
                    emit_bytes(ctx, (const uint8_t*)"\x48\x31\xD8", 3);
                    break;
                default:
                    break;
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_LSHIFT:
        case KRT_IR_RSHIFT: {
            if (inst->operand_count < 2) break;

            KrtIRValue* lhs = &inst->operands[0];
            KrtIRValue* rhs = &inst->operands[1];

            emit_load_value_to_reg(ctx, lhs, REG_RAX);
            emit_load_value_to_reg(ctx, rhs, REG_RCX);

            if (inst->opcode == KRT_IR_LSHIFT) {
                emit_bytes(ctx, (const uint8_t*)"\x48\xD3\xE0", 3);
            } else {
                emit_bytes(ctx, (const uint8_t*)"\x48\xD3\xF8", 3);
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_LT:
        case KRT_IR_GT:
        case KRT_IR_EQ:
        case KRT_IR_LE:
        case KRT_IR_GE:
        case KRT_IR_NE: {
            if (inst->operand_count < 2) break;

            KrtIRValue* lhs = &inst->operands[0];
            KrtIRValue* rhs = &inst->operands[1];

            emit_load_value_to_reg(ctx, lhs, REG_RAX);
            emit_load_value_to_reg(ctx, rhs, REG_RBX);

            emit_bytes(ctx, (const uint8_t*)"\x48\x39\xD8", 3);

            uint8_t setcc_byte;
            switch (inst->opcode) {
                case KRT_IR_LT: setcc_byte = 0x9C; break;
                case KRT_IR_GT: setcc_byte = 0x9F; break;
                case KRT_IR_EQ: setcc_byte = 0x94; break;
                case KRT_IR_LE: setcc_byte = 0x9E; break;
                case KRT_IR_GE: setcc_byte = 0x9D; break;
                case KRT_IR_NE: setcc_byte = 0x95; break;
                default: setcc_byte = 0x94; break;
            }
            emit_byte(ctx, 0x0F);
            emit_byte(ctx, setcc_byte);
            emit_byte(ctx, 0xC0);
            emit_bytes(ctx, (const uint8_t*)"\x48\x0F\xB6\xC0", 4);

            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_POW: {
            if (inst->operand_count < 2) break;

            KrtIRValue* base = &inst->operands[0];
            KrtIRValue* exp = &inst->operands[1];

            emit_load_value_to_reg(ctx, base, g_arg_regs[0]);
            emit_load_value_to_reg(ctx, exp, g_arg_regs[1]);

            int32_t sym_idx = kro_find_symbol(ctx->writer, "KrtPow");
            if (sym_idx < 0) {
                sym_idx = kro_add_undefined_symbol(ctx->writer, "KrtPow");
            }
            if (sym_idx >= 0) {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0xB8);
                uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                emit_u64(ctx, 0);
                emit_byte(ctx, 0xFF);
                emit_byte(ctx, 0xD0);
                kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_ABS64, 0);
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_BRANCH: {
            if (inst->operand_count < 3) break;

            KrtIRValue* cond = &inst->operands[0];
            emit_load_value_to_reg(ctx, cond, REG_RAX);

            emit_bytes(ctx, (const uint8_t*)"\x48\x85\xC0", 3);

            uint32_t jne_offset = kro_get_code_offset(ctx->writer);
            emit_byte(ctx, 0x0F);
            emit_byte(ctx, 0x85);
            emit_u32(ctx, 0);

            uint32_t jmp_offset = kro_get_code_offset(ctx->writer);
            emit_byte(ctx, 0xE9);
            emit_u32(ctx, 0);

            uint32_t false_label_offset = kro_get_code_offset(ctx->writer);

            if (inst->operands[1].type == KRT_IR_VALUE_VAR) {
                int32_t true_sym = kro_find_symbol(ctx->writer, inst->operands[1].data.name);
                if (true_sym < 0) true_sym = kro_add_undefined_symbol(ctx->writer, inst->operands[1].data.name);
                if (true_sym >= 0) {
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, jne_offset + 2, true_sym, KRO_RELOC_PC32, -4);
                }
            }

            if (inst->operands[2].type == KRT_IR_VALUE_VAR) {
                int32_t false_sym = kro_find_symbol(ctx->writer, inst->operands[2].data.name);
                if (false_sym < 0) false_sym = kro_add_undefined_symbol(ctx->writer, inst->operands[2].data.name);
                if (false_sym >= 0) {
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, jmp_offset + 1, false_sym, KRO_RELOC_PC32, -4);
                }
            }
            break;
        }

        case KRT_IR_JUMP: {
            if (inst->operand_count < 1) break;

            if (inst->operands[0].type == KRT_IR_VALUE_VAR) {
                int32_t target_sym = kro_find_symbol(ctx->writer, inst->operands[0].data.name);
                if (target_sym < 0) target_sym = kro_add_undefined_symbol(ctx->writer, inst->operands[0].data.name);
                if (target_sym >= 0) {
                    uint32_t jmp_offset = kro_get_code_offset(ctx->writer);
                    emit_byte(ctx, 0xE9);
                    emit_u32(ctx, 0);
                    kro_add_reloc(ctx->writer, KRO_SEC_TEXT, jmp_offset + 1, target_sym, KRO_RELOC_PC32, -4);
                }
            }
            break;
        }

        case KRT_IR_LABEL: {
            break;
        }

        case KRT_IR_LOADPTR: {
            if (inst->operand_count < 1) break;

            KrtIRValue* base = &inst->operands[0];
            emit_load_value_to_reg(ctx, base, REG_RAX);

            int offset = 0;
            if (inst->operand_count >= 2 && inst->operands[1].type == KRT_IR_VALUE_IMM) {
                offset = (int)inst->operands[1].data.imm;
            }

            if (offset == 0) {
                emit_bytes(ctx, (const uint8_t*)"\x48\x8B\x00", 3);
            } else {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0x8B);
                emit_byte(ctx, 0x80);
                emit_u32(ctx, (uint32_t)offset);
            }

            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_STOREPTR: {
            if (inst->operand_count < 2) break;

            KrtIRValue* base = &inst->operands[0];

            int offset = 0;
            if (inst->operand_count >= 2 && inst->operands[1].type == KRT_IR_VALUE_IMM) {
                offset = (int)inst->operands[1].data.imm;
            }

            KrtIRValue* value = NULL;
            if (inst->operand_count >= 3) {
                value = &inst->operands[2];
            } else if (inst->operand_count >= 2 && inst->operands[1].type != KRT_IR_VALUE_IMM) {
                value = &inst->operands[1];
            }

            if (!value) break;

            emit_load_value_to_reg(ctx, value, REG_RBX);
            emit_load_value_to_reg(ctx, base, REG_RAX);

            if (offset == 0) {
                emit_bytes(ctx, (const uint8_t*)"\x48\x89\x18", 3);
            } else {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0x89);
                emit_byte(ctx, 0x98);
                emit_u32(ctx, (uint32_t)offset);
            }
            break;
        }

        case KRT_IR_ARRAY_STORE: {
            if (inst->operand_count < 3) break;

            KrtIRValue* arr = &inst->operands[0];
            KrtIRValue* index = &inst->operands[1];
            KrtIRValue* value = &inst->operands[2];

            emit_load_value_to_reg(ctx, arr, REG_RCX);
            emit_load_value_to_reg(ctx, index, REG_RDX);
            emit_load_value_to_reg(ctx, value, REG_RAX);

            emit_bytes(ctx, (const uint8_t*)"\x48\x89\x04\xD1", 4);
            break;
        }

        case KRT_IR_CAST: {
            if (inst->operand_count < 1) break;

            KrtIRValue* src = &inst->operands[0];
            emit_load_value_to_reg(ctx, src, REG_RAX);

            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_STRCAT: {
            if (inst->operand_count < 2) break;

            KrtIRValue* lhs = &inst->operands[0];
            KrtIRValue* rhs = &inst->operands[1];

            emit_load_value_to_reg(ctx, lhs, g_arg_regs[0]);
            emit_load_value_to_reg(ctx, rhs, g_arg_regs[1]);

            int32_t sym_idx = kro_find_symbol(ctx->writer, "KrtStringConcat");
            if (sym_idx < 0) {
                sym_idx = kro_add_undefined_symbol(ctx->writer, "KrtStringConcat");
            }
            if (sym_idx >= 0) {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0xB8);
                uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                emit_u64(ctx, 0);
                emit_byte(ctx, 0xFF);
                emit_byte(ctx, 0xD0);
                kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_ABS64, 0);
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_INT_TO_STRING: {
            if (inst->operand_count < 1) break;

            KrtIRValue* val = &inst->operands[0];
            emit_load_value_to_reg(ctx, val, g_arg_regs[0]);

            int32_t sym_idx = kro_find_symbol(ctx->writer, "KrtIntToString");
            if (sym_idx < 0) {
                sym_idx = kro_add_undefined_symbol(ctx->writer, "KrtIntToString");
            }
            if (sym_idx >= 0) {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0xB8);
                uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                emit_u64(ctx, 0);
                emit_byte(ctx, 0xFF);
                emit_byte(ctx, 0xD0);
                kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_ABS64, 0);
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_DOUBLE_TO_STRING: {
            if (inst->operand_count < 1) break;

            KrtIRValue* val = &inst->operands[0];
            emit_load_value_to_reg(ctx, val, g_arg_regs[0]);

            int32_t sym_idx = kro_find_symbol(ctx->writer, "KrtDoubleToString");
            if (sym_idx < 0) {
                sym_idx = kro_add_undefined_symbol(ctx->writer, "KrtDoubleToString");
            }
            if (sym_idx >= 0) {
                emit_byte(ctx, 0x48);
                emit_byte(ctx, 0xB8);
                uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                emit_u64(ctx, 0);
                emit_byte(ctx, 0xFF);
                emit_byte(ctx, 0xD0);
                kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_ABS64, 0);
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_CALL: {
            if (inst->operand_count < 1 || inst->operands[0].type != KRT_IR_VALUE_FUNCTION) break;

            const char* func_name = inst->operands[0].data.function_name;
            if (!func_name) break;

            int arg_count = inst->operand_count - 1;
            int capped_arg_count = arg_count < g_arg_reg_count ? arg_count : g_arg_reg_count;

            for (int i = 0; i < capped_arg_count; i++) {
                KrtIRValue* arg = &inst->operands[i + 1];
                int target_reg = g_arg_regs[i];

                if (arg->type == KRT_IR_VALUE_IMM) {
                    emit_load_imm64_to_reg(ctx, (uint64_t)arg->data.imm, target_reg);
                } else if (arg->type == KRT_IR_VALUE_STRING_CONST) {
                    int32_t sym_idx = -1;
                    if (arg->data.string_const_id >= 0 &&
                        ctx->string_const_sym_indices &&
                        arg->data.string_const_id < ctx->string_const_count) {
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
#if KRO_DEBUG
            fprintf(stderr, "[KroCodegen] CALL: func_name=%s, local_sym_idx=%d\n", func_name, local_sym_idx);
#endif
            if (local_sym_idx >= 0) {
                emit_call_local(ctx, func_name);
            } else {
#if KRO_DEBUG
                fprintf(stderr, "[KroCodegen]   -> external call\n");
#endif
                emit_call_external(ctx, func_name);
            }

            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
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
                } else if (ret_val->type == KRT_IR_VALUE_VAR) {
                    KROLocalVar* local_var = find_local_var(ctx, ret_val->data.name);
                    if (local_var && local_var->allocated) {
                        emit_load_from_stack(ctx, local_var->stack_offset, REG_RAX);
                    } else {
                        int sym_idx = kro_find_symbol(ctx->writer, ret_val->data.name);
                        if (sym_idx >= 0) {
                            emit_byte(ctx, 0x48);
                            emit_byte(ctx, 0x8B);
                            emit_byte(ctx, 0x05);
                            uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                            emit_u32(ctx, 0);
                            kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                        }
                    }
                }
            }
            break;
        }

        case KRT_IR_STORE: {
            if (inst->operand_count < 2) break;

            KrtIRValue* dest = &inst->operands[0];
            KrtIRValue* value = &inst->operands[1];

            if (dest->type == KRT_IR_VALUE_VAR) {
                KROLocalVar* local_var = find_local_var(ctx, dest->data.name);

                if (local_var && local_var->allocated) {
                    int src_reg = REG_RAX;
                    if (value->type == KRT_IR_VALUE_IMM) {
                        emit_load_imm64_to_reg(ctx, (uint64_t)value->data.imm, REG_RAX);
                    } else if (value->type == KRT_IR_VALUE_VAR) {
                        KROLocalVar* src_var = find_local_var(ctx, value->data.name);
                        if (src_var && src_var->allocated) {
                            emit_load_from_stack(ctx, src_var->stack_offset, REG_RAX);
                        }
                    }
                    emit_store_to_stack(ctx, local_var->stack_offset, src_reg);
                } else {
                    int sym_idx = kro_find_symbol(ctx->writer, dest->data.name);
                    if (sym_idx >= 0) {
                        if (value->type == KRT_IR_VALUE_IMM) {
                            emit_load_imm64_to_reg(ctx, (uint64_t)value->data.imm, REG_RAX);
                        }
                        emit_byte(ctx, 0x48);
                        emit_byte(ctx, 0x89);
                        emit_byte(ctx, 0x05);
                        uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                        emit_u32(ctx, 0);
                        kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                    }
                }
            }
            break;
        }

        case KRT_IR_LOAD: {
            if (inst->operand_count < 1) break;

            KrtIRValue* src = &inst->operands[0];

            if (src->type == KRT_IR_VALUE_VAR) {
                KROLocalVar* local_var = find_local_var(ctx, src->data.name);

                if (local_var && local_var->allocated) {
                    emit_load_from_stack(ctx, local_var->stack_offset, REG_RAX);
                } else {
                    int sym_idx = kro_find_symbol(ctx->writer, src->data.name);
                    if (sym_idx >= 0) {
                        emit_byte(ctx, 0x48);
                        emit_byte(ctx, 0x8B);
                        emit_byte(ctx, 0x05);
                        uint32_t reloc_offset = kro_get_code_offset(ctx->writer);
                        emit_u32(ctx, 0);
                        kro_add_reloc(ctx->writer, KRO_SEC_TEXT, reloc_offset, sym_idx, KRO_RELOC_PC32, 0);
                    }
                }
            }
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_ALLOC: {
            if (inst->operand_count < 1) break;

            KrtIRValue* var = &inst->operands[0];
            if (var->type == KRT_IR_VALUE_VAR) {
                find_or_alloc_local_var(ctx, var->data.name);
            }
            break;
        }

        case KRT_IR_COPY: {
            if (inst->operand_count < 1) break;

            KrtIRValue* src = &inst->operands[0];
            KrtIRValue* dst = &inst->result;

            emit_load_value_to_reg(ctx, src, REG_RAX);

            if (dst->type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, dst->data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            } else if (dst->type == KRT_IR_VALUE_VAR) {
                KROLocalVar* local_var = find_local_var(ctx, dst->data.name);
                if (!local_var) {
                    local_var = find_or_alloc_local_var(ctx, dst->data.name);
                }
                if (local_var && local_var->allocated) {
                    emit_store_to_stack(ctx, local_var->stack_offset, REG_RAX);
                }
            }
            break;
        }

        case KRT_IR_PHI: {
            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0 && inst->operand_count >= 1) {
                    emit_load_value_to_reg(ctx, &inst->operands[0], REG_RAX);
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            } else if (inst->result.type == KRT_IR_VALUE_VAR) {
                if (inst->operand_count >= 1) {
                    emit_load_value_to_reg(ctx, &inst->operands[0], REG_RAX);
                    KROLocalVar* local_var = find_local_var(ctx, inst->result.data.name);
                    if (!local_var) {
                        local_var = find_or_alloc_local_var(ctx, inst->result.data.name);
                    }
                    if (local_var && local_var->allocated) {
                        emit_store_to_stack(ctx, local_var->stack_offset, REG_RAX);
                    }
                }
            }
            break;
        }

        case KRT_IR_SYSCALL: {
#if KRO_DEBUG
            fprintf(stderr, "[KroCodegen] KRT_IR_SYSCALL: operand_count=%d\n", inst->operand_count);
            for (int i = 0; i < inst->operand_count && i < 7; i++) {
                KrtIRValue* op = &inst->operands[i];
                fprintf(stderr, "[KroCodegen]   operand[%d]: type=%d", i, op->type);
                if (op->type == KRT_IR_VALUE_IMM) fprintf(stderr, ", imm=%f", op->data.imm);
                if (op->type == KRT_IR_VALUE_TEMP) fprintf(stderr, ", temp_idx=%d", op->data.index);
                if (op->type == KRT_IR_VALUE_VAR) fprintf(stderr, ", var=%s", op->data.name ? op->data.name : "(null)");
                fprintf(stderr, "\n");
            }
#endif
            if (inst->operand_count < 1) break;
            KrtIRValue* syscall_num = &inst->operands[0];
            int arg_count = inst->operand_count - 1;
            if (arg_count > 6) arg_count = 6;

            for (int i = arg_count - 1; i >= 0; i--) {
                KrtIRValue* arg = &inst->operands[i + 1];
                int target_reg = g_arg_regs[i];
                if (arg->type == KRT_IR_VALUE_IMM) {
                    emit_load_imm64_to_reg(ctx, (uint64_t)arg->data.imm, target_reg);
                } else if (arg->type == KRT_IR_VALUE_STRING_CONST) {
                    int32_t sym_idx = -1;
                    if (arg->data.string_const_id >= 0 && ctx->string_const_sym_indices &&
                        arg->data.string_const_id < ctx->string_const_count) {
                        sym_idx = ctx->string_const_sym_indices[arg->data.string_const_id];
                    }
                    if (sym_idx >= 0) {
                        emit_load_string_addr_to_reg(ctx, sym_idx, target_reg);
                    } else {
                        emit_load_imm64_to_reg(ctx, 0, target_reg);
                    }
                } else if (arg->type == KRT_IR_VALUE_VAR) {
                    KROLocalVar* lv = find_local_var(ctx, arg->data.name);
                    if (lv && lv->allocated) {
                        emit_load_from_stack(ctx, lv->stack_offset, target_reg);
                    } else {
                        emit_load_imm64_to_reg(ctx, 0, target_reg);
                    }
                } else if (arg->type == KRT_IR_VALUE_TEMP) {
                    int soff = find_temp_slot(ctx, arg->data.index);
                    if (soff > 0) {
                        emit_load_from_stack(ctx, soff, target_reg);
                    } else {
                        emit_load_imm64_to_reg(ctx, 0, target_reg);
                    }
                } else if (arg->type == KRT_IR_VALUE_ARG) {
                    int aidx = arg->data.index;
                    if (aidx >= 0 && aidx < g_arg_reg_count && g_arg_regs[aidx] != target_reg) {
                        if (target_reg < 8 && g_arg_regs[aidx] < 8) {
                            emit_byte(ctx, 0x48);
                            emit_byte(ctx, 0x89);
                            emit_byte(ctx, 0xC0 | (g_arg_regs[aidx] << 3) | target_reg);
                        }
                    }
                } else {
                    emit_load_value_to_reg(ctx, arg, target_reg);
                }
            }

            if (syscall_num->type == KRT_IR_VALUE_IMM) {
                emit_load_imm64_to_reg(ctx, (uint64_t)syscall_num->data.imm, REG_RAX);
            } else if (syscall_num->type == KRT_IR_VALUE_VAR) {
                KROLocalVar* lv = find_local_var(ctx, syscall_num->data.name);
                if (lv && lv->allocated) {
                    emit_load_from_stack(ctx, lv->stack_offset, REG_RAX);
                } else {
                    emit_load_imm64_to_reg(ctx, 0, REG_RAX);
                }
            } else if (syscall_num->type == KRT_IR_VALUE_TEMP) {
                int soff = find_temp_slot(ctx, syscall_num->data.index);
                if (soff > 0) {
                    emit_load_from_stack(ctx, soff, REG_RAX);
                } else {
                    emit_load_imm64_to_reg(ctx, 0, REG_RAX);
                }
            } else if (syscall_num->type == KRT_IR_VALUE_ARG) {
                int aidx = syscall_num->data.index;
                if (aidx >= 0 && aidx < g_arg_reg_count && g_arg_regs[aidx] != REG_RAX) {
                    if (REG_RAX < 8 && g_arg_regs[aidx] < 8) {
                        emit_byte(ctx, 0x48);
                        emit_byte(ctx, 0x89);
                        emit_byte(ctx, 0xC0 | (g_arg_regs[aidx] << 3));
                    }
                }
            } else {
                emit_load_value_to_reg(ctx, syscall_num, REG_RAX);
            }

#if KRO_DEBUG
            fprintf(stderr, "[KroCodegen] Emitting syscall bytes at offset %u\n", kro_get_code_offset(ctx->writer));
#endif
            emit_byte(ctx, 0x0F);
            emit_byte(ctx, 0x05);
#if KRO_DEBUG
            fprintf(stderr, "[KroCodegen] Syscall bytes emitted\n");
#endif

            if (inst->result.type == KRT_IR_VALUE_TEMP) {
                int slot = alloc_temp_slot(ctx, inst->result.data.index);
                if (slot > 0) {
                    emit_store_to_stack(ctx, slot, REG_RAX);
                }
            }
            break;
        }

        default:
            fprintf(stderr, "[KroCodegen] WARNING: Unhandled opcode %d\n", inst->opcode);
            break;
    }
}

static void KroGenerateBlock(KROCodegenContext* ctx, KrtIRBasicBlock* block) {
    if (!block) return;

#if KRO_DEBUG
    int count = 0;
#endif
    KrtIRInst* inst = block->first_inst;
    while (inst) {
        uint32_t before_offset = kro_get_code_offset(ctx->writer);
        KroGenerateInstruction(ctx, inst);
#if KRO_DEBUG
        uint32_t after_offset = kro_get_code_offset(ctx->writer);
        fprintf(stderr, "[KroCodegen]   Instruction[%d]: opcode=%d, bytes_emitted=%u\n",
                count, inst->opcode, after_offset - before_offset);
        count++;
#endif
        inst = inst->next;
    }
}

static int calculate_function_stack_size(KrtIRFunction* func) {
    int stack_size = KRO_SHADOW_SPACE_SIZE + 8;

    if (func) {
        int local_var_count = 0;
        KrtIRBasicBlock* block = func->entry_block;

        while (block) {
            for (int i = 0; i < block->inst_count; i++) {
                KrtIRInst* inst = block->insts[i];
                if (!inst) continue;

                if (inst->opcode == KRT_IR_ALLOC && inst->operand_count >= 1 &&
                    inst->operands[0].type == KRT_IR_VALUE_VAR) {
                    local_var_count++;
                }
            }
            block = block->next;
        }
        stack_size += local_var_count * 8;
    }

    stack_size = (stack_size + 15) & ~15;
    if ((stack_size % 16) != 8) {
        stack_size += 8;
    }
    if (stack_size < KRO_MIN_STACK_SIZE) stack_size = KRO_MIN_STACK_SIZE;
    return stack_size;
}

static void KroGenerateFunction(KROCodegenContext* ctx, KrtIRFunction* func, KrtIRModule* module) {
    if (!func) return;

    (void)kro_get_code_offset(ctx->writer);

#if KRO_DEBUG
    fprintf(stderr, "[KroCodegen] KroGenerateFunction: name=%s, entry_block=%p\n",
            func->name ? func->name : "(null)", (void*)func->entry_block);
    if (func->entry_block) {
        fprintf(stderr, "[KroCodegen]   Entry block details BEFORE generation:\n");
        fprintf(stderr, "   - first_inst=%p\n", (void*)func->entry_block->first_inst);
        fprintf(stderr, "   - inst_count=%d\n", func->entry_block->inst_count);

        fprintf(stderr, "   - Linked list traversal:\n");
        int dbg_count = 0;
        KrtIRInst* dbg_inst = func->entry_block->first_inst;
        while (dbg_inst) {
            fprintf(stderr, "     [%d] opcode=%d, addr=%p, next=%p\n",
                    dbg_count, dbg_inst->opcode, (void*)dbg_inst, (void*)dbg_inst->next);
            dbg_count++;
            dbg_inst = dbg_inst->next;
        }
        fprintf(stderr, "   - Total in linked list: %d\n", dbg_count);

        if (func->entry_block->insts) {
            fprintf(stderr, "   - Array contents:\n");
            for (int i = 0; i < func->entry_block->inst_count; i++) {
                KrtIRInst* arr_inst = func->entry_block->insts[i];
                fprintf(stderr, "     [%d] opcode=%d, addr=%p\n", i,
                        arr_inst ? arr_inst->opcode : -1,
                        arr_inst ? (void*)arr_inst : NULL);
            }
        }
    }
#endif

    bool has_mangled_main = kro_module_has_mangled_main(module);
    int is_entry_point = is_entry_point_function(func->name, has_mangled_main);

    ctx->is_main_func = is_entry_point;
    ctx->local_var_count = 0;
    ctx->current_stack_offset = 8;
    memset(ctx->local_vars, 0, sizeof(ctx->local_vars));
    memset(ctx->temp_slots, 0, sizeof(ctx->temp_slots));
    ctx->temp_slot_count = 0;
    ctx->func_index++;

    int stack_size = calculate_function_stack_size(func);

    emit_function_prologue(ctx, stack_size);

    bool has_explicit_return = false;
    KrtIRBasicBlock* block = func->entry_block;
    while (block) {
        KrtIRInst* scan_inst = block->first_inst;
        while (scan_inst) {
            if (scan_inst->opcode == KRT_IR_RETURN) {
                has_explicit_return = true;
                break;
            }
            scan_inst = scan_inst->next;
        }
        KroGenerateBlock(ctx, block);
        block = block->next;
    }

    if (!has_explicit_return) {
        emit_load_imm64(ctx, 0);
    }

    emit_function_epilogue(ctx);
}

static uint32_t escape_string(const char* input, char* output, uint32_t output_size) {
    if (!input || !output || output_size == 0) return 0;

    uint32_t out_pos = 0;
    const char* p = input;

    while (*p && out_pos < output_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n':  output[out_pos++] = '\n'; break;
                case 'r':  output[out_pos++] = '\r'; break;
                case 't':  output[out_pos++] = '\t'; break;
                case '\\': output[out_pos++] = '\\'; break;
                case '"':  output[out_pos++] = '"';  break;
                case '0':  output[out_pos++] = '\0'; break;
                case 'a':  output[out_pos++] = '\a'; break;
                case 'b':  output[out_pos++] = '\b'; break;
                case 'f':  output[out_pos++] = '\f'; break;
                case 'v':  output[out_pos++] = '\v'; break;
                default:
                    output[out_pos++] = '\\';
                    if (out_pos < output_size - 1) {
                        output[out_pos++] = *p;
                    }
                    break;
            }
        } else {
            output[out_pos++] = *p;
        }
        p++;
    }

    output[out_pos] = '\0';
    return out_pos + 1;
}

static void KroGenerateDataSection(KROCodegenContext* ctx, KrtIRModule* module) {
    if (!module || !ctx || !ctx->writer) return;

    if (module->string_const_count > 0 && module->string_constants) {
        ctx->string_const_sym_indices = (int32_t*)KRT_MALLOC(module->string_const_count * sizeof(int32_t));
        if (!ctx->string_const_sym_indices) return;
        ctx->string_const_count = module->string_const_count;

        for (int i = 0; i < module->string_const_count; i++) {
            const char* str = module->string_constants[i];
            if (!str) continue;
            uint32_t rodata_offset = kro_get_rodata_offset(ctx->writer);

            char escaped[4096];
            uint32_t escaped_len = escape_string(str, escaped, sizeof(escaped));
            kro_write_rodata(ctx->writer, escaped, escaped_len);

            char sym_name[64];
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
        size_t len = strlen(output_filename);
        if (len >= sizeof(ctx.output_filename)) len = sizeof(ctx.output_filename) - 1;
        memcpy(ctx.output_filename, output_filename, len);
        ctx.output_filename[len] = '\0';
    } else {
        memcpy(ctx.output_filename, "output.kro", 11);
    }
    ctx.writer = kro_writer_create();

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

    if (func_count == 0) {
        kro_writer_destroy(ctx.writer);
        if (ctx.string_const_sym_indices) {
            KRT_FREE(ctx.string_const_sym_indices);
        }
        return;
    }

    int* sym_indices = (int*)KRT_MALLOC(func_count * sizeof(int));
    if (!sym_indices) {
        kro_writer_destroy(ctx.writer);
        if (ctx.string_const_sym_indices) {
            KRT_FREE(ctx.string_const_sym_indices);
        }
        return;
    }

    int func_idx = 0;
#if KRO_DEBUG
     fprintf(stderr, "[KroCodegen] Module functions:\n");
     KrtIRFunction* dbg_func = module->functions;
     while (dbg_func) {
         fprintf(stderr, "[KroCodegen]   func: %s\n", dbg_func->name ? dbg_func->name : "(null)");
         dbg_func = dbg_func->next;
     }
#endif

     bool has_mangled_main = kro_module_has_mangled_main(module);

     KrtIRFunction* func = module->functions;
     while (func) {
         uint32_t func_offset = kro_get_code_offset(ctx.writer);
        int sym_idx = kro_add_symbol(ctx.writer, func->name, KRO_SYM_FUNC, KRO_BIND_GLOBAL, KRO_SEC_TEXT, func_offset);
        sym_indices[func_idx] = sym_idx;

        if (is_entry_point_function(func->name, has_mangled_main)) {
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

#if KRO_DEBUG
        fprintf(stderr, "[KroCodegen] Function '%s': actual_offset=0x%x, sym_idx=%d\n",
                func->name ? func->name : "(null)",
                actual_offset,
                sym_indices[func_idx]);
#endif

        if (sym_indices[func_idx] >= 0) {
            kro_update_symbol_value(ctx.writer, sym_indices[func_idx], actual_offset);
        }

        if (is_entry_point_function(func->name, has_mangled_main)) {
            kro_set_entry_point(ctx.writer, actual_offset);
        }

        KroGenerateFunction(&ctx, func, module);
        func = func->next;
        func_idx++;
    }

    KRT_FREE(sym_indices);

    if (!kro_write_file(ctx.writer, ctx.output_filename)) {
    }

    if (ctx.string_const_sym_indices) {
        KRT_FREE(ctx.string_const_sym_indices);
    }
    kro_writer_destroy(ctx.writer);
}
