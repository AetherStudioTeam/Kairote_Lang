#include "ArkLink/Loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KRO_DEBUG  // 调试输出开关

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t entry_point;
    uint64_t code_size;
    uint64_t data_size;
    uint64_t rodata_size;
    uint64_t symbol_count;
    uint64_t reloc_count;
} KroHeader;

typedef struct {
    uint32_t name_offset;
    uint32_t section;
    uint64_t value;
    uint64_t size;
    uint32_t binding;
    uint32_t visibility;
    uint32_t import_module_offset;  
} KroSymbol;

typedef struct {
    uint32_t offset;
    uint32_t type;
    uint32_t sym_idx;
    int32_t addend;
    uint32_t section_index;
} KroReloc;

#pragma pack(pop)

#define KRO_MAGIC 0x4F524B41

static const char* kro_string_at(const char* string_table, size_t table_size, uint32_t offset) {
    if (!string_table || offset >= (uint32_t)table_size) return NULL;
    size_t max_len = table_size - offset;
    if (!memchr(string_table + offset, '\0', max_len)) return NULL;
    return string_table + offset;
}

ArkLinkResult ark_link_load_kro(const char* path, ArkLinkUnit** unit) {
#ifdef KRO_DEBUG
    fprintf(stderr, "[LoaderKro] Loading KRO file: %s\n", path ? path : "(null)");
#endif
    if (!path || !unit) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return ARK_LINK_ERR_NOT_FOUND;
    }

    KroHeader header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return ARK_LINK_ERR_FORMAT;
    }

    if (header.magic != KRO_MAGIC) {
        fclose(file);
        return ARK_LINK_ERR_FORMAT;
    }

    ArkLinkUnit* new_unit = ark_link_unit_create(path);
    if (!new_unit) {
        fclose(file);
        return ARK_LINK_ERR_MEMORY;
    }

    size_t header_size = sizeof(KroHeader);
    size_t code_offset = header_size;
    size_t data_offset = code_offset + header.code_size;
    size_t rodata_offset = data_offset + header.data_size;
    size_t symbol_offset = rodata_offset + header.rodata_size;
    size_t reloc_offset = symbol_offset + header.symbol_count * sizeof(KroSymbol);
    
    if (header.code_size > 0) {
        uint8_t* code_data = (uint8_t*)malloc((size_t)header.code_size);
        if (!code_data) {
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        fseek(file, (long)code_offset, SEEK_SET);
        if (fread(code_data, 1, (size_t)header.code_size, file) != (size_t)header.code_size) {
            free(code_data);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_IO;
        }

        ArkSectionDesc desc = {0};
        desc.name = ".text";
        desc.data = code_data;
        desc.size = (size_t)header.code_size;
        desc.alignment = 0x1000;
        desc.flags = ARK_SECTION_READ | ARK_SECTION_EXEC;

        ArkLinkSection* sec = ark_link_unit_add_section(new_unit, &desc);
        if (!sec) {
            free(code_data);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        sec->kind = ARK_SECTION_CODE;
    }

    if (header.data_size > 0) {
        uint8_t* data_buf = (uint8_t*)malloc((size_t)header.data_size);
        if (!data_buf) {
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        fseek(file, (long)data_offset, SEEK_SET);
        if (fread(data_buf, 1, (size_t)header.data_size, file) != (size_t)header.data_size) {
            free(data_buf);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_IO;
        }

        ArkSectionDesc desc = {0};
        desc.name = ".data";
        desc.data = data_buf;
        desc.size = (size_t)header.data_size;
        desc.alignment = 0x1000;
        desc.flags = ARK_SECTION_READ | ARK_SECTION_WRITE;

        ArkLinkSection* sec = ark_link_unit_add_section(new_unit, &desc);
        if (!sec) {
            free(data_buf);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        sec->kind = ARK_SECTION_DATA;
    }

    if (header.rodata_size > 0) {
        uint8_t* rodata_buf = (uint8_t*)malloc((size_t)header.rodata_size);
        if (!rodata_buf) {
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        fseek(file, (long)rodata_offset, SEEK_SET);
        if (fread(rodata_buf, 1, (size_t)header.rodata_size, file) != (size_t)header.rodata_size) {
            free(rodata_buf);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_IO;
        }

        ArkSectionDesc desc = {0};
        desc.name = ".rodata";
        desc.data = rodata_buf;
        desc.size = (size_t)header.rodata_size;
        desc.alignment = 0x1000;
        desc.flags = ARK_SECTION_READ;

        ArkLinkSection* sec = ark_link_unit_add_section(new_unit, &desc);
        if (!sec) {
            free(rodata_buf);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        sec->kind = ARK_SECTION_RODATA;
    }

#ifdef KRO_DEBUG
    fprintf(stderr, "[LoaderKro] header.symbol_count = %zu\n", (size_t)header.symbol_count);
#endif
    if (header.symbol_count > 0) {
        
        KroSymbol* kro_syms = (KroSymbol*)malloc(header.symbol_count * sizeof(KroSymbol));
        if (!kro_syms) {
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_MEMORY;
        }

        fseek(file, (long)symbol_offset, SEEK_SET);
        if (fread(kro_syms, sizeof(KroSymbol), (size_t)header.symbol_count, file) != (size_t)header.symbol_count) {
            free(kro_syms);
            ark_link_unit_destroy(new_unit);
            fclose(file);
            return ARK_LINK_ERR_IO;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        long string_table_offset = (long)(reloc_offset + header.reloc_count * sizeof(KroReloc));
        long string_table_size = file_size - string_table_offset;

        char* string_table = NULL;
        if (string_table_size > 0) {
            string_table = (char*)malloc((size_t)string_table_size);
            if (string_table) {
                fseek(file, string_table_offset, SEEK_SET);
                fread(string_table, 1, (size_t)string_table_size, file);
            }
        }

        uint32_t kro_to_actual[5] = {0};
#ifdef KRO_DEBUG
        fprintf(stderr, "[LoaderKro] Section mapping for unit with %zu sections:\n", new_unit->section_count);
#endif
        for (size_t s = 0; s < new_unit->section_count; s++) {
            ArkLinkSection* sec = &new_unit->sections[s];
#ifdef KRO_DEBUG
            fprintf(stderr, "  Section[%zu]: name=%s\n", s, sec->name ? sec->name : "(null)");
#endif
            if (strcmp(sec->name, ".text") == 0) kro_to_actual[1] = (uint32_t)(s + 1);
            else if (strcmp(sec->name, ".data") == 0) kro_to_actual[2] = (uint32_t)(s + 1);
            else if (strcmp(sec->name, ".rodata") == 0) kro_to_actual[3] = (uint32_t)(s + 1);
            else if (strcmp(sec->name, ".bss") == 0) kro_to_actual[4] = (uint32_t)(s + 1);
        }
#ifdef KRO_DEBUG
        fprintf(stderr, "[LoaderKro] KRO to actual section map: [0]=%u [1]=%u [2]=%u [3]=%u [4]=%u\n",
                kro_to_actual[0], kro_to_actual[1], kro_to_actual[2], kro_to_actual[3], kro_to_actual[4]);
#endif

        for (size_t i = 0; i < (size_t)header.symbol_count; i++) {
            const char* sym_name = kro_string_at(string_table, (size_t)string_table_size, kro_syms[i].name_offset);
            const char* import_module = (kro_syms[i].import_module_offset != 0)
                ? kro_string_at(string_table, (size_t)string_table_size, kro_syms[i].import_module_offset)
                : NULL;

            uint32_t actual_section = 0;
            if (kro_syms[i].section > 0 && kro_syms[i].section <= 4) {
                actual_section = kro_to_actual[kro_syms[i].section];
            }

#ifdef KRO_DEBUG
            fprintf(stderr, "[LoaderKro] Symbol[%zu]: name=%s, kro_section=%u, actual_section=%u, value=0x%lx\n",
                    i,
                    sym_name ? sym_name : "(null)",
                    kro_syms[i].section,
                    actual_section,
                    (unsigned long)kro_syms[i].value);
#endif

            ArkSymbolDesc sym_desc = {0};
            sym_desc.name = sym_name ? strdup(sym_name) : NULL;
            sym_desc.section_index = actual_section;
            sym_desc.value = kro_syms[i].value;
            sym_desc.size = (uint32_t)kro_syms[i].size;
            sym_desc.binding = (ArkSymbolBinding)kro_syms[i].binding;
            sym_desc.visibility = (ArkSymbolVisibility)kro_syms[i].visibility;
            sym_desc.import_module = import_module ? strdup(import_module) : NULL;

            if (actual_section > 0 && actual_section <= new_unit->section_count) {
                ArkLinkSection* sec = &new_unit->sections[actual_section - 1];
                if (sec->kind == ARK_SECTION_CODE) {
                    sym_desc.type = ARK_SYM_FUNC;
#ifdef KRO_DEBUG
                    fprintf(stderr, "[LoaderKro] Symbol '%s' in .text section, setting type=FUNC\n",
                            sym_name ? sym_name : "(null)");
#endif
                } else {
                    sym_desc.type = ARK_SYM_OBJECT;
                }
            }

            ark_link_unit_add_symbol(new_unit, &sym_desc);
        }

        free(kro_syms);
        free(string_table);

        if (header.reloc_count > 0 && new_unit->section_count > 0) {
            KroReloc* kro_relocs = (KroReloc*)malloc(header.reloc_count * sizeof(KroReloc));
            if (kro_relocs) {
                fseek(file, (long)reloc_offset, SEEK_SET);
                if (fread(kro_relocs, sizeof(KroReloc), (size_t)header.reloc_count, file) == (size_t)header.reloc_count) {
                    for (size_t i = 0; i < (size_t)header.reloc_count; i++) {
                        ArkRelocationDesc reloc_desc = {0};
                        reloc_desc.offset = kro_relocs[i].offset;
                        reloc_desc.type = kro_relocs[i].type;
                        reloc_desc.sym_idx = kro_relocs[i].sym_idx;
                        reloc_desc.addend = kro_relocs[i].addend;

                        uint32_t kro_sec_idx = kro_relocs[i].section_index;
                        uint32_t actual_sec_idx = 0;
                        if (kro_sec_idx > 0 && kro_sec_idx <= 4) {
                            actual_sec_idx = kro_to_actual[kro_sec_idx];
                        }
                        if (actual_sec_idx > 0 && actual_sec_idx <= new_unit->section_count) {
                            reloc_desc.section_index = actual_sec_idx - 1;
                            ark_link_section_add_reloc(&new_unit->sections[actual_sec_idx - 1], &reloc_desc);
                        }
                    }
                }
                free(kro_relocs);
            }
        }
    }

    fclose(file);
    *unit = new_unit;
    return ARK_LINK_OK;
}