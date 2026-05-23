#ifndef ARKLINK_BACKEND_H
#define ARKLINK_BACKEND_H

#include "context.h"
#include "resolver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArkImportEntry {
    const char* module;
    const char* symbol;
    uint32_t iat_rva;
} ArkImportEntry;

typedef struct ArkExportEntry {
    const char* name;
    uint32_t ordinal;
    uint32_t section_index;
    uint32_t offset;
    uint64_t value;
    int is_function;
} ArkExportEntry;

typedef struct ArkBackendInput {
    ArkSectionBuffer* sections;
    size_t section_count;
    uint64_t entry_point;
    uint32_t entry_section;
    uint32_t entry_offset;
    ArkImportEntry* imports;
    size_t import_count;
    ArkExportEntry* exports;
    size_t export_count;
    const char* export_name;
    uint32_t export_ordinal_base;
    ArkResolverReloc* relocs;
    size_t reloc_count;
    uint64_t image_base;
} ArkBackendInput;

typedef struct ArkSectionRvaMap {
    uint32_t rva;
    uint32_t size;
    uint32_t file_offset;
    uint32_t flags;
} ArkSectionRvaMap;

typedef struct ArkBackendOutput {
    uint8_t* data;
    size_t size;
    ArkSectionRvaMap* section_maps;
    size_t section_count;
    uint64_t image_base;
} ArkBackendOutput;

typedef ArkLinkResult (*ArkBackendLinkFn)(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output);

ArkLinkResult ark_backend_pe_link(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output);

#ifdef __cplusplus
}
#endif

#endif 