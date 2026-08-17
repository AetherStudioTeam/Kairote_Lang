#include "ArkLink/Resolver.h"
#include "ArkLink/Context.h"
#include "ArkLink/Backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ArkResolverSymbol* symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} SymbolTable;

typedef struct {
    ArkResolverReloc* relocs;
    size_t reloc_count;
    size_t reloc_capacity;
} RelocTable;

static int symbol_table_init(SymbolTable* table, size_t initial_capacity) {
    table->symbols = (ArkResolverSymbol*)malloc(initial_capacity * sizeof(ArkResolverSymbol));
    if (!table->symbols) return 0;
    table->symbol_count = 0;
    table->symbol_capacity = initial_capacity;
    return 1;
}

static void symbol_table_free(SymbolTable* table) {
    if (table->symbols) {
        free(table->symbols);
    }
}

static int symbol_table_add(SymbolTable* table, const ArkResolverSymbol* sym) {

    fprintf(stderr, "[symbol_table_add] Adding symbol: name=%s, is_export=%d, section=%u\n",
            sym->name ? sym->name : "(null)",
            sym->is_export,
            sym->section_index);

    for (size_t i = 0; i < table->symbol_count; i++) {
        if (strcmp(table->symbols[i].name, sym->name) == 0) {

            fprintf(stderr, "[symbol_table_add] Duplicate found: existing section=%u, new section=%u\n",
                    table->symbols[i].section_index,
                    sym->section_index);

            if (table->symbols[i].section_index == 0 && sym->section_index > 0) {

                fprintf(stderr, "[symbol_table_add] Overwriting with new symbol (is_export=%d)\n", sym->is_export);
                table->symbols[i] = *sym;
            } else if (table->symbols[i].section_index > 0 && sym->section_index > 0) {

                return 0;
            } else {

            }
            return 1;
        }
    }
    
    if (table->symbol_count >= table->symbol_capacity) {
        size_t new_capacity = table->symbol_capacity * 2;
        ArkResolverSymbol* new_symbols = (ArkResolverSymbol*)realloc(table->symbols, new_capacity * sizeof(ArkResolverSymbol));
        if (!new_symbols) return 0;
        table->symbols = new_symbols;
        table->symbol_capacity = new_capacity;
    }
    table->symbols[table->symbol_count++] = *sym;
    return 1;
}

static int reloc_table_init(RelocTable* table, size_t initial_capacity) {
    table->relocs = (ArkResolverReloc*)malloc(initial_capacity * sizeof(ArkResolverReloc));
    if (!table->relocs) return 0;
    table->reloc_count = 0;
    table->reloc_capacity = initial_capacity;
    return 1;
}

static void reloc_table_free(RelocTable* table) {
    if (table->relocs) {
        free(table->relocs);
    }
}

static int reloc_table_add(RelocTable* table, const ArkResolverReloc* reloc) {
    if (table->reloc_count >= table->reloc_capacity) {
        size_t new_capacity = table->reloc_capacity * 2;
        ArkResolverReloc* new_relocs = (ArkResolverReloc*)realloc(table->relocs, new_capacity * sizeof(ArkResolverReloc));
        if (!new_relocs) return 0;
        table->relocs = new_relocs;
        table->reloc_capacity = new_capacity;
    }
    table->relocs[table->reloc_count++] = *reloc;
    return 1;
}

static ArkResolverSymbol* find_symbol(SymbolTable* table, const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < table->symbol_count; i++) {
        const char* sym_name = table->symbols[i].name;
        if (sym_name && strcmp(sym_name, name) == 0) {
            return &table->symbols[i];
        }
    }
    return NULL;
}

ArkLinkResult ark_resolver_resolve(ArkLinkContext* ctx, ArkLinkUnit* const* units, size_t unit_count, ArkResolverPlan* out_plan) {
    if (!ctx || !units || unit_count == 0 || !out_plan) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }

    memset(out_plan, 0, sizeof(ArkResolverPlan));

    SymbolTable sym_table;
    if (!symbol_table_init(&sym_table, 64)) {
        return ARK_LINK_ERR_MEMORY;
    }

    RelocTable reloc_table;
    if (!reloc_table_init(&reloc_table, 64)) {
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    uint32_t* unit_sym_start = (uint32_t*)calloc(unit_count, sizeof(uint32_t));
    if (!unit_sym_start) {
        reloc_table_free(&reloc_table);
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    uint32_t* unit_sec_start = (uint32_t*)calloc(unit_count, sizeof(uint32_t));
    if (!unit_sec_start) {
        free(unit_sym_start);
        reloc_table_free(&reloc_table);
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    size_t sec_idx_counter = 1;
    for (size_t i = 0; i < unit_count; i++) {
        ArkLinkUnit* unit = units[i];
        if (!unit) continue;
        unit_sec_start[i] = (uint32_t)sec_idx_counter;
        sec_idx_counter += unit->section_count;
    }

    for (size_t i = 0; i < unit_count; i++) {
        ArkLinkUnit* unit = units[i];
        if (!unit) continue;

        unit_sym_start[i] = (uint32_t)sym_table.symbol_count;

        for (size_t j = 0; j < unit->symbol_count; j++) {
            ArkSymbolDesc* sym_desc = &unit->symbols[j];

            fprintf(stderr, "[Resolver] Raw symbol from unit[%zu][%zu]: name=%s, raw_section=%u\n",
                    i, j,
                    sym_desc->name ? sym_desc->name : "(null)",
                    sym_desc->section_index);

            ArkResolverSymbol sym = {0};
            sym.name = sym_desc->name;
            sym.binding = sym_desc->binding;
            sym.visibility = sym_desc->visibility;

            if (sym_desc->section_index > 0) {
                sym.section_index = unit_sec_start[i] + sym_desc->section_index - 1;
                fprintf(stderr, "[Resolver] Calculated section: unit_sec_start[%zu]=%u + raw_section=%u - 1 = %u\n",
                        i, unit_sec_start[i], sym_desc->section_index, sym.section_index);
            } else {
                sym.section_index = 0;

                if (sym.name && strcmp(sym.name, "main") == 0) {
                    for (size_t s = 0; s < unit->section_count; s++) {
                        ArkLinkSection* sec = &unit->sections[s];
                        if (sec->kind == ARK_SECTION_CODE) {
                            sym.section_index = unit_sec_start[i] + s;
                            fprintf(stderr, "[Resolver] Fixing main function section: %u -> %u\n",
                                    0, sym.section_index);
                            break;
                        }
                    }
                }
            }
            sym.value = (uint32_t)sym_desc->value;
            sym.size = sym_desc->size;
            sym.type = sym_desc->type;
            sym.import_module = sym_desc->import_module;

            if (sym_desc->binding == ARK_BIND_GLOBAL) {
                sym.is_export = 1;
            }

            fprintf(stderr, "[Resolver] Symbol[%zu]: name=%s, binding=%d, section=%u, import_module=%s, is_export=%d\n",
                    j,
                    sym.name ? sym.name : "(null)",
                    (int)sym.binding,
                    sym.section_index,
                    sym.import_module ? sym.import_module : "(null)",
                    sym.is_export);

            if (!symbol_table_add(&sym_table, &sym)) {
                free(unit_sec_start);
                free(unit_sym_start);
                reloc_table_free(&reloc_table);
                symbol_table_free(&sym_table);
                return ARK_LINK_ERR_DUPLICATE_SYMBOL;
            }
        }
    }

    out_plan->backend_input = (ArkBackendInput*)calloc(1, sizeof(ArkBackendInput));
    if (!out_plan->backend_input) {
        free(unit_sec_start);
        free(unit_sym_start);
        reloc_table_free(&reloc_table);
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    size_t total_sections = 0;
    for (size_t i = 0; i < unit_count; i++) {
        if (units[i]) {
            total_sections += units[i]->section_count;
        }
    }

    if (total_sections == 0) {
        reloc_table_free(&reloc_table);
        free(out_plan->backend_input);
        symbol_table_free(&sym_table);
        return ARK_LINK_OK;
    }

    out_plan->backend_input->sections = (ArkSectionBuffer*)calloc(total_sections, sizeof(ArkSectionBuffer));
    if (!out_plan->backend_input->sections) {
        reloc_table_free(&reloc_table);
        free(out_plan->backend_input);
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    uint32_t** section_map = (uint32_t**)calloc(unit_count, sizeof(uint32_t*));
    if (!section_map) {
        reloc_table_free(&reloc_table);
        free(out_plan->backend_input->sections);
        free(out_plan->backend_input);
        symbol_table_free(&sym_table);
        return ARK_LINK_ERR_MEMORY;
    }

    size_t sec_idx = 0;
    for (size_t i = 0; i < unit_count; i++) {
        ArkLinkUnit* unit = units[i];
        if (!unit) {
            section_map[i] = NULL;
            continue;
        }

        section_map[i] = (uint32_t*)calloc(unit->section_count, sizeof(uint32_t));
        if (!section_map[i]) {
            for (size_t k = 0; k < i; k++) {
                free(section_map[k]);
            }
            free(section_map);
            free(unit_sec_start);
            free(unit_sym_start);
            reloc_table_free(&reloc_table);
            free(out_plan->backend_input->sections);
            free(out_plan->backend_input);
            symbol_table_free(&sym_table);
            return ARK_LINK_ERR_MEMORY;
        }

        for (size_t j = 0; j < unit->section_count; j++) {
            ArkLinkSection* src = &unit->sections[j];
            ArkSectionBuffer* dst = &out_plan->backend_input->sections[sec_idx];

            dst->data = (uint8_t*)malloc(src->size);
            if (dst->data && src->data) {
                memcpy(dst->data, src->data, src->size);
            }
            dst->size = src->size;
            dst->capacity = src->size;
            dst->kind = (int)src->kind;
            dst->flags = src->flags;
            dst->alignment = (uint32_t)src->alignment;



            section_map[i][j] = (uint32_t)sec_idx;
            sec_idx++;
        }
    }

    out_plan->backend_input->section_count = total_sections;

    out_plan->backend_input->entry_section = 1;
    out_plan->backend_input->entry_offset = 0;

    ArkResolverSymbol* entry_sym = find_symbol(&sym_table, "_start");
    fprintf(stderr, "[Resolver] Looking for entry point symbol...\n");
    if (!entry_sym) {
        entry_sym = find_symbol(&sym_table, "main");
        fprintf(stderr, "[Resolver]   _start not found, trying main: %p\n", (void*)entry_sym);
        if (!entry_sym) {
            entry_sym = find_symbol(&sym_table, "_ZN4mainEv");
            fprintf(stderr, "[Resolver]   main not found, trying _ZN4mainEv: %p\n", (void*)entry_sym);
        }
        if (!entry_sym) {
            entry_sym = find_symbol(&sym_table, "_KrtMainEntry");
            fprintf(stderr, "[Resolver]   _ZN4mainEv not found, trying _KrtMainEntry: %p\n", (void*)entry_sym);
        }
    }

    if (entry_sym) {

        if (entry_sym->section_index > 0) {
            out_plan->backend_input->entry_section = entry_sym->section_index;
        } else {
            out_plan->backend_input->entry_section = 1;
        }

        out_plan->backend_input->entry_offset = entry_sym->value;
        fprintf(stderr, "[Resolver] Found entry point: name=%s, section=%u, value=0x%x\n",
                entry_sym->name ? entry_sym->name : "(null)",
                entry_sym->section_index,
                entry_sym->value);
        fprintf(stderr, "[Resolver] Setting entry: section=%u, offset=0x%x (total_sections=%zu)\n",
                out_plan->backend_input->entry_section,
                out_plan->backend_input->entry_offset,
                (size_t)total_sections);
    } else {
        fprintf(stderr, "[Resolver] WARNING: No entry point symbol found!\n");
    }

    for (size_t i = 0; i < sym_table.symbol_count; i++) {
        ArkResolverSymbol* sym = &sym_table.symbols[i];
        if (sym->section_index < total_sections) {
            sym->section = &out_plan->backend_input->sections[sym->section_index];
        }
    }

    for (size_t i = 0; i < unit_count; i++) {
        ArkLinkUnit* unit = units[i];
        if (!unit) continue;

        for (size_t j = 0; j < unit->section_count; j++) {
            ArkLinkSection* sec = &unit->sections[j];
            uint32_t global_sec_idx = section_map[i][j];

            for (size_t k = 0; k < sec->reloc_count; k++) {
                ArkRelocationDesc* reloc_desc = &sec->relocs[k];

                ArkResolverReloc reloc = {0};
                reloc.section = &out_plan->backend_input->sections[global_sec_idx];
                reloc.section_index = global_sec_idx;
                reloc.original_section_index = (uint32_t)j;
                reloc.offset = (uint32_t)reloc_desc->offset;
                reloc.type = reloc_desc->type;
                reloc.addend = reloc_desc->addend;

                if (reloc_desc->sym_idx < unit->symbol_count) {
                    const char* sym_name = unit->symbols[reloc_desc->sym_idx].name;
                    ArkResolverSymbol* global_sym = find_symbol(&sym_table, sym_name);
                    if (global_sym) {
                        reloc.symbol = global_sym;
                    }
                }

                if (!reloc_table_add(&reloc_table, &reloc)) {
                    for (size_t m = 0; m < unit_count; m++) {
                        free(section_map[m]);
                    }
                    free(section_map);
                    free(unit_sec_start);
                    free(unit_sym_start);
                    reloc_table_free(&reloc_table);
                    free(out_plan->backend_input->sections);
                    free(out_plan->backend_input);
                    symbol_table_free(&sym_table);
                    return ARK_LINK_ERR_MEMORY;
                }
            }
        }
    }

    out_plan->relocs = reloc_table.relocs;
    out_plan->reloc_count = reloc_table.reloc_count;

    out_plan->backend_input->relocs = reloc_table.relocs;
    out_plan->backend_input->reloc_count = reloc_table.reloc_count;
    out_plan->backend_input->image_base = 0x140000000;

    size_t import_count = 0;
    for (size_t i = 0; i < sym_table.symbol_count; i++) {
        if (sym_table.symbols[i].import_module && sym_table.symbols[i].import_module[0] != '\0') {
            import_count++;
        }
    }

    if (import_count > 0) {
        ArkImportEntry* imports = (ArkImportEntry*)calloc(import_count, sizeof(ArkImportEntry));
        if (imports) {
            size_t idx = 0;
            for (size_t i = 0; i < sym_table.symbol_count; i++) {
                if (sym_table.symbols[i].import_module) {
                    imports[idx].module = strdup(sym_table.symbols[i].import_module);
                    imports[idx].symbol = strdup(sym_table.symbols[i].name);
                    imports[idx].iat_rva = 0;

                    imports[idx].is_function = (sym_table.symbols[i].type == ARK_SYM_FUNC);

                    fprintf(stderr, "[Resolver]   Import[%zu]: %s.%s (%s)\n",
                            idx,
                            imports[idx].module ? imports[idx].module : "(null)",
                            imports[idx].symbol,
                            imports[idx].is_function ? "FUNC" : "DATA");

                    idx++;
                }
            }
            out_plan->backend_input->imports = imports;
            out_plan->backend_input->import_count = import_count;
        }
    }

    size_t export_count = 0;
    int has_main_export = 0;

    fprintf(stderr, "[Resolver] Counting exports from %zu symbols (addr=%p)...\n",
            sym_table.symbol_count, (void*)sym_table.symbols);
    for (size_t i = 0; i < sym_table.symbol_count; i++) {
        ArkResolverSymbol* s = &sym_table.symbols[i];
        fprintf(stderr, "[Resolver]   Symbol[%zu]@%p: name=%s, is_export=%d, import_module=%s, binding=%d\n",
                i,
                (void*)s,
                s->name ? s->name : "(null)",
                s->is_export,
                s->import_module ? s->import_module : "(null)",
                (int)s->binding);

        int is_import = s->import_module && strlen(s->import_module) > 0;
        if (s->is_export && !is_import) {
            export_count++;
            fprintf(stderr, "[Resolver]     -> PASS: is_export=%d, import_module=%s\n",
                    s->is_export,
                    s->import_module ? s->import_module : "(null)");
            fprintf(stderr, "[Resolver]     -> counted as export (total=%zu)\n", export_count);
        } else {
            fprintf(stderr, "[Resolver]     -> FAIL: is_export=%d, is_import=%d\n",
                    s->is_export,
                    is_import);
        }

        if (!has_main_export &&
            sym_table.symbols[i].name &&
            strcmp(sym_table.symbols[i].name, "main") == 0 &&
            !(sym_table.symbols[i].import_module && strlen(sym_table.symbols[i].import_module) > 0)) {

            has_main_export = 1;
            if (!sym_table.symbols[i].is_export) {
                export_count++;
                fprintf(stderr, "[Resolver]     -> main not exported, adding (total=%zu)\n", export_count);
            }
        }
    }

    fprintf(stderr, "[Resolver] Final export_count=%zu\n", export_count);

    if (export_count > 0) {
        ArkExportEntry* exports = (ArkExportEntry*)calloc(export_count, sizeof(ArkExportEntry));
        if (exports) {
            size_t idx = 0;
            for (size_t i = 0; i < sym_table.symbol_count; i++) {
                ArkResolverSymbol* sym = &sym_table.symbols[i];

                int is_import_sym = sym->import_module && strlen(sym->import_module) > 0;
                int should_export = sym->is_export && !is_import_sym;

                if (!should_export &&
                    sym->name &&
                    strcmp(sym->name, "main") == 0 &&
                    !is_import_sym) {

                    should_export = 1;
                    fprintf(stderr, "[Resolver] Force-exporting 'main' function (entry point)\n");
                }

                if (should_export) {
                    exports[idx].name = strdup(sym->name);
                    exports[idx].ordinal = 0;
                    exports[idx].section_index = sym->section_index;
                    exports[idx].offset = sym->value;
                    exports[idx].value = sym->value;
                    exports[idx].is_function = (sym->type == ARK_SYM_FUNC);

                    fprintf(stderr, "[Resolver]   Export[%zu]: %s (sec=%u, off=0x%x, %s)\n",
                            idx,
                            exports[idx].name ? exports[idx].name : "(null)",
                            exports[idx].section_index,
                            exports[idx].offset,
                            exports[idx].is_function ? "FUNC" : "DATA");

                    idx++;
                }
            }
            out_plan->backend_input->exports = exports;
            out_plan->backend_input->export_count = export_count;

            fprintf(stderr, "[Resolver] Total exports: %zu\n", export_count);
        }
    }

    for (size_t i = 0; i < unit_count; i++) {
        free(section_map[i]);
    }
    free(section_map);
    free(unit_sec_start);
    free(unit_sym_start);

    out_plan->symbols = sym_table.symbols;
    out_plan->symbol_count = sym_table.symbol_count;
    out_plan->relocs = reloc_table.relocs;
    out_plan->reloc_count = reloc_table.reloc_count;

    return ARK_LINK_OK;
}

void ark_resolver_plan_destroy(ArkLinkContext* ctx, ArkResolverPlan* plan) {
    (void)ctx;
    if (!plan) return;

    if (plan->symbols) {
        free(plan->symbols);
        plan->symbols = NULL;
    }

    if (plan->relocs) {
        free(plan->relocs);
        plan->relocs = NULL;
    }

    if (plan->import_modules) {
        free(plan->import_modules);
        plan->import_modules = NULL;
    }

    if (plan->imports) {
        free(plan->imports);
        plan->imports = NULL;
    }

    if (plan->backend_input && plan->backend_input->imports) {
        for (size_t i = 0; i < plan->backend_input->import_count; i++) {
            free((void*)plan->backend_input->imports[i].module);
            free((void*)plan->backend_input->imports[i].symbol);
        }
        free(plan->backend_input->imports);
        plan->backend_input->imports = NULL;
    }

    if (plan->backend_input && plan->backend_input->exports) {
        for (size_t i = 0; i < plan->backend_input->export_count; i++) {
            free((void*)plan->backend_input->exports[i].name);
        }
        free(plan->backend_input->exports);
        plan->backend_input->exports = NULL;
    }

    if (plan->exports) {
        free(plan->exports);
        plan->exports = NULL;
    }

    if (plan->backend_input) {
        
        if (plan->backend_input->sections) {
            for (size_t i = 0; i < plan->backend_input->section_count; i++) {
                if (plan->backend_input->sections[i].data) {
                    free(plan->backend_input->sections[i].data);
                }
            }
            free(plan->backend_input->sections);
        }
        free(plan->backend_input);
        plan->backend_input = NULL;
    }

    memset(plan, 0, sizeof(ArkResolverPlan));
}