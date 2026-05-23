#include "ArkLink/backend_pe.h"
#include "ArkLink/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#pragma pack(push, 1)

typedef struct {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
} PE_DOS_HEADER;

#define PE_SIGNATURE 0x00004550

typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} PE_COFF_HEADER;

#define PE_OPT_HDR_MAGIC_PE32 0x10b
#define PE_OPT_HDR_MAGIC_PE32_PLUS 0x20b

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} PE_DATA_DIRECTORY;

typedef struct {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    PE_DATA_DIRECTORY DataDirectory[16];
} PE_OPTIONAL_HEADER_64;

typedef struct {
    uint8_t Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} PE_SECTION_HEADER;

#pragma pack(pop)
#define PE_MACHINE_AMD64 0x8664
#define PE_CHAR_EXECUTABLE_IMAGE 0x0002
#define PE_CHAR_LARGE_ADDRESS_AWARE 0x0020
#define PE_CHAR_DEBUG_STRIPPED 0x0200
#define PE_SCN_CNT_CODE 0x00000020
#define PE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define PE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define PE_SCN_MEM_EXECUTE 0x20000000
#define PE_SCN_MEM_READ 0x40000000
#define PE_SCN_MEM_WRITE 0x80000000
#define PE_SUBSYSTEM_WINDOWS_CUI 3
#define PE_DD_EXPORT 0
#define PE_DD_IMPORT 1
#define PE_DD_RESOURCE 2
#define PE_DD_EXCEPTION 3
#define PE_DD_CERTIFICATE 4
#define PE_DD_BASE_RELOCATION 5
#define PE_DD_DEBUG 6
#define PE_DD_ARCHITECTURE 7
#define PE_DD_GLOBAL_PTR 8
#define PE_DD_TLS 9
#define PE_DD_LOAD_CONFIG 10
#define PE_DD_BOUND_IMPORT 11
#define PE_DD_IAT 12
#define PE_DD_DELAY_IMPORT 13
#define PE_DD_COM_DESCRIPTOR 14
#define PE_REL_BASED_ABS 0
#define PE_REL_BASED_HIGH 1
#define PE_REL_BASED_LOW 2
#define PE_REL_BASED_HIGHLOW 3
#define PE_REL_BASED_HIGHADJ 4
#define PE_REL_BASED_DIR64 10

typedef struct {
    uint32_t ImportLookupTableRVA;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t ImportAddressTableRVA;
} PE_IMPORT_DIRECTORY_ENTRY;

typedef struct {
    uint64_t Value;
} PE_IMPORT_LOOKUP_ENTRY;

typedef struct {
    uint16_t Hint;
    char Name[1];  
} PE_HINT_NAME_ENTRY;

typedef struct {
    uint32_t PageRVA;
    uint32_t BlockSize;
    
} PE_BASE_RELOCATION_BLOCK;

typedef struct {
    uint32_t ExportFlags;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t NameRVA;
    uint32_t OrdinalBase;
    uint32_t AddressTableEntries;
    uint32_t NumberOfNamePointers;
    uint32_t ExportAddressTableRVA;
    uint32_t NamePointerRVA;
    uint32_t OrdinalTableRVA;
} PE_EXPORT_DIRECTORY_ENTRY;

static void write_dos_header(uint8_t* data) {
    PE_DOS_HEADER* dos = (PE_DOS_HEADER*)data;
    
    dos->e_magic = 0x5A4D; 
    dos->e_cblp = 0x0090;
    dos->e_cp = 0x0003;
    dos->e_cparhdr = 0x0004;
    dos->e_minalloc = 0x0000;
    dos->e_maxalloc = 0xFFFF;
    dos->e_ss = 0x0000;
    dos->e_sp = 0x00B8;
    dos->e_csum = 0x0000;
    dos->e_ip = 0x0000;
    dos->e_cs = 0x0000;
    dos->e_lfarlc = 0x0040;
    dos->e_ovno = 0x0000;
    dos->e_lfanew = 0x00000080; 
    
    static const uint8_t dos_stub[] = {
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
        0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
        0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
        0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
        0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
        0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(data + sizeof(PE_DOS_HEADER), dos_stub, sizeof(dos_stub));
}

static void write_pe_headers(uint8_t* data, size_t section_count, uint32_t image_size,
                             uint32_t entry_point_rva, uint32_t section_alignment,
                             uint32_t file_alignment, uint32_t reloc_rva, uint32_t reloc_size) {
    uint32_t pe_offset = 0x80;
    
    *(uint32_t*)(data + pe_offset) = PE_SIGNATURE;
    
    PE_COFF_HEADER* coff = (PE_COFF_HEADER*)(data + pe_offset + 4);
    coff->Machine = PE_MACHINE_AMD64;
    coff->NumberOfSections = (uint16_t)section_count;
    coff->TimeDateStamp = 0;
    coff->PointerToSymbolTable = 0;
    coff->NumberOfSymbols = 0;
    coff->SizeOfOptionalHeader = sizeof(PE_OPTIONAL_HEADER_64);
    coff->Characteristics = PE_CHAR_EXECUTABLE_IMAGE | PE_CHAR_LARGE_ADDRESS_AWARE | PE_CHAR_DEBUG_STRIPPED;
    
    PE_OPTIONAL_HEADER_64* opt = (PE_OPTIONAL_HEADER_64*)(data + pe_offset + 4 + sizeof(PE_COFF_HEADER));
    opt->Magic = PE_OPT_HDR_MAGIC_PE32_PLUS;
    opt->MajorLinkerVersion = 1;
    opt->MinorLinkerVersion = 0;
    opt->SizeOfCode = 0;
    opt->SizeOfInitializedData = 0;
    opt->SizeOfUninitializedData = 0;
    opt->AddressOfEntryPoint = entry_point_rva;
    opt->BaseOfCode = section_alignment;
    opt->ImageBase = 0x140000000;
    opt->SectionAlignment = section_alignment;
    opt->FileAlignment = file_alignment;
    opt->MajorOperatingSystemVersion = 4;
    opt->MinorOperatingSystemVersion = 0;
    opt->MajorImageVersion = 0;
    opt->MinorImageVersion = 0;
    opt->MajorSubsystemVersion = 5;
    opt->MinorSubsystemVersion = 2;
    opt->Win32VersionValue = 0;
    opt->SizeOfImage = image_size;
    opt->SizeOfHeaders = file_alignment;
    opt->CheckSum = 0;
    opt->Subsystem = PE_SUBSYSTEM_WINDOWS_CUI;
    opt->DllCharacteristics = 0;
    opt->SizeOfStackReserve = 0x100000;
    opt->SizeOfStackCommit = 0x1000;
    opt->SizeOfHeapReserve = 0x100000;
    opt->SizeOfHeapCommit = 0x1000;
    opt->LoaderFlags = 0;
    opt->NumberOfRvaAndSizes = 16;
    
    for (int i = 0; i < 16; i++) {
        opt->DataDirectory[i].VirtualAddress = 0;
        opt->DataDirectory[i].Size = 0;
    }
    
    if (reloc_size > 0) {
        opt->DataDirectory[PE_DD_BASE_RELOCATION].VirtualAddress = reloc_rva;
        opt->DataDirectory[PE_DD_BASE_RELOCATION].Size = reloc_size;
    }
}

static void write_section_header(uint8_t* data, size_t index, const char* name,
                                  uint32_t virtual_size, uint32_t virtual_address,
                                  uint32_t size_of_raw_data, uint32_t pointer_to_raw_data,
                                  uint32_t characteristics) {
    uint32_t pe_offset = 0x80;
    uint32_t section_table_offset = pe_offset + 4 + sizeof(PE_COFF_HEADER) + sizeof(PE_OPTIONAL_HEADER_64);
    
    PE_SECTION_HEADER* sec = (PE_SECTION_HEADER*)(data + section_table_offset + index * sizeof(PE_SECTION_HEADER));
    
    memset(sec->Name, 0, 8);
    size_t name_len = strlen(name);
    if (name_len > 8) name_len = 8;
    memcpy(sec->Name, name, name_len);
    
    sec->VirtualSize = virtual_size;
    sec->VirtualAddress = virtual_address;
    sec->SizeOfRawData = size_of_raw_data;
    sec->PointerToRawData = pointer_to_raw_data;
    sec->PointerToRelocations = 0;
    sec->PointerToLinenumbers = 0;
    sec->NumberOfRelocations = 0;
    sec->NumberOfLinenumbers = 0;
    sec->Characteristics = characteristics;
}

static uint8_t* generate_relocation_table(ArkBackendInput* input, ArkSectionRvaMap* section_maps, 
                                          size_t* out_size) {
    if (!input->relocs || input->reloc_count == 0) {
        *out_size = 0;
        return NULL;
    }

    #define PAGE_SIZE 0x1000

    uint32_t* page_rvas = (uint32_t*)malloc(input->reloc_count * sizeof(uint32_t));
    if (!page_rvas) return NULL;

    size_t page_count = 0;
    for (size_t i = 0; i < input->reloc_count; i++) {
        ArkResolverReloc* reloc = &input->relocs[i];
        if (!reloc->symbol) continue;
        
        if (reloc->type == ARK_RELOC_PC32) continue;

        uint32_t reloc_rva = section_maps[reloc->section_index].rva + reloc->offset;
        uint32_t page_rva = reloc_rva & ~(PAGE_SIZE - 1);

        int found = 0;
        for (size_t j = 0; j < page_count; j++) {
            if (page_rvas[j] == page_rva) {
                found = 1;
                break;
            }
        }
        if (!found) {
            page_rvas[page_count++] = page_rva;
        }
    }

    if (page_count == 0) {
        free(page_rvas);
        *out_size = 0;
        return NULL;
    }

    size_t total_size = 0;
    for (size_t p = 0; p < page_count; p++) {
        size_t count = 0;
        for (size_t i = 0; i < input->reloc_count; i++) {
            ArkResolverReloc* reloc = &input->relocs[i];
            if (!reloc->symbol) continue;
            
            if (reloc->type == ARK_RELOC_PC32) continue;
            uint32_t reloc_rva = section_maps[reloc->section_index].rva + reloc->offset;
            uint32_t page_rva = reloc_rva & ~(PAGE_SIZE - 1);
            if (page_rva == page_rvas[p]) {
                count++;
            }
        }
        size_t block_size = 8 + ((count + 1) & ~1) * 2; 
        total_size += block_size;
    }

    uint8_t* reloc_data = (uint8_t*)calloc(1, total_size);
    if (!reloc_data) {
        free(page_rvas);
        return NULL;
    }

    size_t offset = 0;
    for (size_t p = 0; p < page_count; p++) {
        PE_BASE_RELOCATION_BLOCK* block = (PE_BASE_RELOCATION_BLOCK*)(reloc_data + offset);
        block->PageRVA = page_rvas[p];

        uint16_t* type_offset = (uint16_t*)(reloc_data + offset + 8);
        size_t count = 0;
        for (size_t i = 0; i < input->reloc_count; i++) {
            ArkResolverReloc* reloc = &input->relocs[i];
            if (!reloc->symbol) continue;
            
            if (reloc->type == ARK_RELOC_PC32) continue;
            uint32_t reloc_rva = section_maps[reloc->section_index].rva + reloc->offset;
            uint32_t page_rva = reloc_rva & ~(PAGE_SIZE - 1);
            if (page_rva == page_rvas[p]) {
                uint16_t offset_in_page = reloc_rva - page_rva;
                uint16_t type = PE_REL_BASED_DIR64; 
                if (reloc->type == ARK_RELOC_ADDR32) {
                    type = PE_REL_BASED_HIGHLOW; 
                }
                type_offset[count++] = (type << 12) | (offset_in_page & 0xFFF);
            }
        }

        size_t block_size = 8 + ((count + 1) & ~1) * 2;
        block->BlockSize = (uint32_t)block_size;
        offset += block_size;
    }

    free(page_rvas);
    *out_size = total_size;
    return reloc_data;
}

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    uint32_t rva;
} ImportTableBuilder;

static ImportTableBuilder* import_builder_create(size_t initial_capacity, uint32_t rva) {
    ImportTableBuilder* builder = (ImportTableBuilder*)malloc(sizeof(ImportTableBuilder));
    if (!builder) return NULL;
    
    builder->data = (uint8_t*)calloc(1, initial_capacity);
    if (!builder->data) {
        free(builder);
        return NULL;
    }
    
    builder->size = 0;
    builder->capacity = initial_capacity;
    builder->rva = rva;
    return builder;
}

static void import_builder_free(ImportTableBuilder* builder) {
    if (builder) {
        free(builder->data);
        free(builder);
    }
}

static size_t import_builder_append(ImportTableBuilder* builder, const void* data, size_t len) {
    if (builder->size + len > builder->capacity) {
        size_t new_capacity = builder->capacity * 2;
        while (new_capacity < builder->size + len) {
            new_capacity *= 2;
        }
        uint8_t* new_data = (uint8_t*)realloc(builder->data, new_capacity);
        if (!new_data) return (size_t)-1;
        builder->data = new_data;
        builder->capacity = new_capacity;
    }
    
    size_t offset = builder->size;
    if (data) {
        memcpy(builder->data + offset, data, len);
    } else {
        
        memset(builder->data + offset, 0, len);
    }
    builder->size += len;
    return offset;
}

static uint32_t import_builder_get_rva(ImportTableBuilder* builder, size_t offset) {
    return builder->rva + (uint32_t)offset;
}

static uint8_t* generate_import_table(ArkBackendInput* input, uint32_t idata_rva, 
                                       size_t* out_size, uint32_t* out_iat_rva,
                                       size_t* out_iat_total_entries) {
    if (!input->imports || input->import_count == 0) {
        *out_size = 0;
        *out_iat_rva = 0;
        if (out_iat_total_entries) *out_iat_total_entries = 0;
        return NULL;
    }

    size_t module_count = 0;
    const char** modules = (const char**)calloc(input->import_count, sizeof(char*));
    if (!modules) return NULL;
    
    for (size_t i = 0; i < input->import_count; i++) {
        const char* mod = input->imports[i].module;
        int found = 0;
        for (size_t j = 0; j < module_count; j++) {
            if (strcmp(modules[j], mod) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            modules[module_count++] = mod;
        }
    }

    ImportTableBuilder* builder = import_builder_create(4096, idata_rva);
    if (!builder) {
        free(modules);
        return NULL;
    }

    size_t idt_offset = import_builder_append(builder, NULL, 
        (module_count + 1) * sizeof(PE_IMPORT_DIRECTORY_ENTRY));
    if (idt_offset == (size_t)-1) goto fail;

    PE_IMPORT_DIRECTORY_ENTRY* idt = (PE_IMPORT_DIRECTORY_ENTRY*)(builder->data + idt_offset);
    
    for (size_t m = 0; m < module_count; m++) {
        const char* module_name = modules[m];
        
        size_t sym_count = 0;
        for (size_t i = 0; i < input->import_count; i++) {
            if (strcmp(input->imports[i].module, module_name) == 0) {
                sym_count++;
            }
        }
        
        size_t* sym_indices = (size_t*)malloc(sym_count * sizeof(size_t));
        if (!sym_indices) goto fail;
        
        size_t sym_idx = 0;
        for (size_t i = 0; i < input->import_count; i++) {
            if (strcmp(input->imports[i].module, module_name) == 0) {
                sym_indices[sym_idx++] = i;
            }
        }
        
        size_t name_len = strlen(module_name) + 1;
        size_t name_padded_len = (name_len + 1) & ~1;
        size_t total_hint_name_size = 0;
        for (size_t i = 0; i < sym_count; i++) {
            size_t sym_name_len = strlen(input->imports[sym_indices[i]].symbol) + 1;
            total_hint_name_size += sizeof(uint16_t) + ((sym_name_len + 1) & ~1);
        }
        
        size_t ilt_size = (sym_count + 1) * sizeof(PE_IMPORT_LOOKUP_ENTRY);
        size_t total_size = ilt_size * 2 + name_padded_len + total_hint_name_size;
        
        if (builder->size + total_size > builder->capacity) {
            size_t new_capacity = builder->capacity;
            while (new_capacity < builder->size + total_size) {
                new_capacity *= 2;
            }
            uint8_t* new_data = (uint8_t*)realloc(builder->data, new_capacity);
            if (!new_data) {
                free(sym_indices);
                goto fail;
            }
            builder->data = new_data;
            builder->capacity = new_capacity;
            
            idt = (PE_IMPORT_DIRECTORY_ENTRY*)(builder->data + idt_offset);
        }
        
        size_t ilt_offset = builder->size;
        if (import_builder_append(builder, NULL, ilt_size) == (size_t)-1) {
            free(sym_indices);
            goto fail;
        }
        PE_IMPORT_LOOKUP_ENTRY* ilt = (PE_IMPORT_LOOKUP_ENTRY*)(builder->data + ilt_offset);
        
        size_t iat_offset = builder->size;
        if (import_builder_append(builder, NULL, ilt_size) == (size_t)-1) {
            free(sym_indices);
            goto fail;
        }
        PE_IMPORT_LOOKUP_ENTRY* iat = (PE_IMPORT_LOOKUP_ENTRY*)(builder->data + iat_offset);
        
        size_t name_offset = builder->size;
        
        if (import_builder_append(builder, module_name, name_len) == (size_t)-1) {
            free(sym_indices);
            goto fail;
        }
        
        if (name_len % 2 != 0) {
            uint8_t pad = 0;
            if (import_builder_append(builder, &pad, 1) == (size_t)-1) {
                free(sym_indices);
                goto fail;
            }
        }
        
        for (size_t i = 0; i < sym_count; i++) {
            size_t import_idx = sym_indices[i];
            
            size_t hint_name_offset = builder->size;
            uint16_t hint = 0;  
            if (import_builder_append(builder, &hint, sizeof(hint)) == (size_t)-1) {
                free(sym_indices);
                goto fail;
            }
            
            const char* sym_name = input->imports[import_idx].symbol;
            size_t sym_name_len = strlen(sym_name) + 1;
            if (import_builder_append(builder, sym_name, sym_name_len) == (size_t)-1) {
                free(sym_indices);
                goto fail;
            }
            
            if (sym_name_len % 2 != 0) {
                uint8_t pad = 0;
                if (import_builder_append(builder, &pad, 1) == (size_t)-1) {
                    free(sym_indices);
                    goto fail;
                }
            }
            
            uint64_t entry_value = import_builder_get_rva(builder, hint_name_offset);
            ilt[i].Value = entry_value;
            iat[i].Value = entry_value;
        }
        
        free(sym_indices);
        
        idt[m].ImportLookupTableRVA = import_builder_get_rva(builder, ilt_offset);
        idt[m].TimeDateStamp = 0;
        idt[m].ForwarderChain = 0;
        idt[m].NameRVA = import_builder_get_rva(builder, name_offset);
        idt[m].ImportAddressTableRVA = import_builder_get_rva(builder, iat_offset);
    }
    
    memset(&idt[module_count], 0, sizeof(PE_IMPORT_DIRECTORY_ENTRY));
    
    *out_size = builder->size;
    *out_iat_rva = idt[0].ImportAddressTableRVA;  
    
    if (out_iat_total_entries) {
        *out_iat_total_entries = 0;
        for (size_t m = 0; m < module_count; m++) {
            const char* module_name = modules[m];
            size_t sym_count = 0;
            for (size_t i = 0; i < input->import_count; i++) {
                if (strcmp(input->imports[i].module, module_name) == 0) {
                    sym_count++;
                }
            }
            *out_iat_total_entries += sym_count + 1;  
        }
    }
    
    uint8_t* result = (uint8_t*)malloc(builder->size);
    if (result) {
        memcpy(result, builder->data, builder->size);
    }
    
    import_builder_free(builder);
    free(modules);
    return result;

fail:
    import_builder_free(builder);
    free(modules);
    return NULL;
}

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    uint32_t base_rva;
} ExportTableBuilder;

static ExportTableBuilder* export_builder_create(size_t initial_capacity, uint32_t base_rva) {
    ExportTableBuilder* builder = (ExportTableBuilder*)calloc(1, sizeof(ExportTableBuilder));
    if (!builder) return NULL;
    
    builder->data = (uint8_t*)malloc(initial_capacity);
    if (!builder->data) {
        free(builder);
        return NULL;
    }
    
    builder->capacity = initial_capacity;
    builder->base_rva = base_rva;
    return builder;
}

static void export_builder_free(ExportTableBuilder* builder) {
    if (builder) {
        free(builder->data);
        free(builder);
    }
}

static size_t export_builder_append(ExportTableBuilder* builder, const void* data, size_t size) {
    if (builder->size + size > builder->capacity) {
        size_t new_capacity = builder->capacity * 2;
        uint8_t* new_data = (uint8_t*)realloc(builder->data, new_capacity);
        if (!new_data) return (size_t)-1;
        builder->data = new_data;
        builder->capacity = new_capacity;
    }
    
    size_t offset = builder->size;
    if (data) {
        memcpy(builder->data + offset, data, size);
    } else {
        memset(builder->data + offset, 0, size);
    }
    builder->size += size;
    return offset;
}

static uint32_t export_builder_get_rva(ExportTableBuilder* builder, size_t offset) {
    return builder->base_rva + (uint32_t)offset;
}

static int compare_export_entries(const void* a, const void* b) {
    const ArkExportEntry* ea = (const ArkExportEntry*)a;
    const ArkExportEntry* eb = (const ArkExportEntry*)b;
    return strcmp(ea->name, eb->name);
}

static uint8_t* generate_export_table(ArkBackendInput* input, uint32_t edata_rva,
                                       size_t* out_size, ArkSectionRvaMap* section_maps) {
    if (!input->exports || input->export_count == 0) {
        *out_size = 0;
        return NULL;
    }

    ExportTableBuilder* builder = export_builder_create(4096, edata_rva);
    if (!builder) return NULL;

    ArkExportEntry* sorted_exports = (ArkExportEntry*)malloc(input->export_count * sizeof(ArkExportEntry));
    if (!sorted_exports) goto fail;
    memcpy(sorted_exports, input->exports, input->export_count * sizeof(ArkExportEntry));
    qsort(sorted_exports, input->export_count, sizeof(ArkExportEntry), compare_export_entries);

    size_t edt_offset = export_builder_append(builder, NULL, sizeof(PE_EXPORT_DIRECTORY_ENTRY));
    if (edt_offset == (size_t)-1) goto fail;

    size_t eat_offset = builder->size;
    size_t eat_size = input->export_count * sizeof(uint32_t);
    if (export_builder_append(builder, NULL, eat_size) == (size_t)-1) goto fail;
    uint32_t* eat = (uint32_t*)(builder->data + eat_offset);

    size_t ent_offset = builder->size;
    size_t ent_size = input->export_count * sizeof(uint32_t);
    if (export_builder_append(builder, NULL, ent_size) == (size_t)-1) goto fail;
    uint32_t* ent = (uint32_t*)(builder->data + ent_offset);

    size_t eot_offset = builder->size;
    size_t eot_size = input->export_count * sizeof(uint16_t);
    if (export_builder_append(builder, NULL, eot_size) == (size_t)-1) goto fail;
    uint16_t* eot = (uint16_t*)(builder->data + eot_offset);

    size_t dll_name_offset = builder->size;
    const char* dll_name = input->export_name ? input->export_name : "exported.dll";
    size_t dll_name_len = strlen(dll_name) + 1;
    if (export_builder_append(builder, dll_name, dll_name_len) == (size_t)-1) goto fail;

    for (size_t i = 0; i < input->export_count; i++) {
        ArkExportEntry* exp = &sorted_exports[i];
        
        size_t name_offset = builder->size;
        size_t name_len = strlen(exp->name) + 1;
        if (export_builder_append(builder, exp->name, name_len) == (size_t)-1) goto fail;
        
        ent[i] = export_builder_get_rva(builder, name_offset);
        
        eot[i] = (uint16_t)(exp->ordinal - input->export_ordinal_base);
        
        if (exp->section_index > 0 && exp->section_index <= input->section_count) {
            eat[exp->ordinal - input->export_ordinal_base] = 
                section_maps[exp->section_index].rva + exp->offset;
        } else {
            
            eat[exp->ordinal - input->export_ordinal_base] = (uint32_t)exp->value;
        }
    }

    PE_EXPORT_DIRECTORY_ENTRY* edt = (PE_EXPORT_DIRECTORY_ENTRY*)(builder->data + edt_offset);
    edt->ExportFlags = 0;
    edt->TimeDateStamp = 0;
    edt->MajorVersion = 0;
    edt->MinorVersion = 0;
    edt->NameRVA = export_builder_get_rva(builder, dll_name_offset);
    edt->OrdinalBase = input->export_ordinal_base;
    edt->AddressTableEntries = (uint32_t)input->export_count;
    edt->NumberOfNamePointers = (uint32_t)input->export_count;
    edt->ExportAddressTableRVA = export_builder_get_rva(builder, eat_offset);
    edt->NamePointerRVA = export_builder_get_rva(builder, ent_offset);
    edt->OrdinalTableRVA = export_builder_get_rva(builder, eot_offset);

    *out_size = builder->size;
    
    uint8_t* result = (uint8_t*)malloc(builder->size);
    if (result) {
        memcpy(result, builder->data, builder->size);
    }
    
    free(sorted_exports);
    export_builder_free(builder);
    return result;

fail:
    free(sorted_exports);
    export_builder_free(builder);
    return NULL;
}

ArkLinkResult ark_backend_pe_link(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output) {
    if (!ctx || !input || !output) {
        return ARK_LINK_ERR_INVALID_ARGUMENT;
    }

    memset(output, 0, sizeof(ArkBackendOutput));

    const uint32_t dos_header_size = 0x80;
    const uint32_t pe_signature_size = 4;
    const uint32_t section_alignment = 0x1000;
    const uint32_t file_alignment = 0x200;
    
    size_t reloc_data_size = 0;
    uint8_t* reloc_data = NULL;
    
    ArkSectionRvaMap* temp_maps = (ArkSectionRvaMap*)calloc(input->section_count, sizeof(ArkSectionRvaMap));
    if (!temp_maps) {
        return ARK_LINK_ERR_MEMORY;
    }
    
    uint32_t current_rva = section_alignment;
    for (size_t i = 0; i < input->section_count; i++) {
        temp_maps[i].rva = current_rva;
        temp_maps[i].size = (uint32_t)input->sections[i].size;
        uint32_t aligned_size = (uint32_t)((input->sections[i].size + section_alignment - 1) & ~(section_alignment - 1));
        current_rva += aligned_size;
    }
    
    reloc_data = generate_relocation_table(input, temp_maps, &reloc_data_size);
    
    size_t import_data_size = 0;
    uint32_t import_iat_rva = 0;
    size_t import_iat_total_entries = 0;
    uint8_t* import_data = NULL;
    
    uint32_t idata_rva = current_rva;
    if (input->imports && input->import_count > 0) {
        import_data = generate_import_table(input, idata_rva, &import_data_size, &import_iat_rva, &import_iat_total_entries);
    }
    
    if (import_data_size > 0) {
        uint32_t idata_aligned_size = (uint32_t)((import_data_size + section_alignment - 1) & ~(section_alignment - 1));
        current_rva += idata_aligned_size;
    }
    
    size_t export_data_size = 0;
    uint8_t* export_data = NULL;
    
    uint32_t edata_rva = current_rva;
    if (input->exports && input->export_count > 0) {
        export_data = generate_export_table(input, edata_rva, &export_data_size, temp_maps);
    }
    
    if (export_data_size > 0) {
        uint32_t edata_aligned_size = (uint32_t)((export_data_size + section_alignment - 1) & ~(section_alignment - 1));
        current_rva += edata_aligned_size;
    }
    
    int has_reloc_section = (reloc_data_size > 0);
    int has_idata_section = (import_data_size > 0);
    int has_edata_section = (export_data_size > 0);
    size_t total_sections = input->section_count + (has_idata_section ? 1 : 0) + (has_edata_section ? 1 : 0) + (has_reloc_section ? 1 : 0);
    
    uint32_t pe_header_size = pe_signature_size + sizeof(PE_COFF_HEADER) + sizeof(PE_OPTIONAL_HEADER_64);
    uint32_t section_table_size = (uint32_t)(total_sections * sizeof(PE_SECTION_HEADER));
    uint32_t total_header_size = dos_header_size + pe_header_size + section_table_size;
    
    uint32_t headers_size = (total_header_size + file_alignment - 1) & ~(file_alignment - 1);
    
    uint32_t image_size = section_alignment; 
    for (size_t i = 0; i < input->section_count; i++) {
        uint32_t section_size = (uint32_t)((input->sections[i].size + section_alignment - 1) & ~(section_alignment - 1));
        image_size += section_size;
    }
    
    uint32_t idata_virtual_size = 0;
    uint32_t idata_raw_size = 0;
    if (has_idata_section) {
        idata_virtual_size = (uint32_t)((import_data_size + section_alignment - 1) & ~(section_alignment - 1));
        idata_raw_size = (uint32_t)((import_data_size + file_alignment - 1) & ~(file_alignment - 1));
        image_size += idata_virtual_size;
    }
    
    uint32_t edata_virtual_size = 0;
    uint32_t edata_raw_size = 0;
    if (has_edata_section) {
        edata_virtual_size = (uint32_t)((export_data_size + section_alignment - 1) & ~(section_alignment - 1));
        edata_raw_size = (uint32_t)((export_data_size + file_alignment - 1) & ~(file_alignment - 1));
        image_size += edata_virtual_size;
    }
    
    uint32_t reloc_virtual_size = 0;
    uint32_t reloc_raw_size = 0;
    if (has_reloc_section) {
        reloc_virtual_size = (uint32_t)((reloc_data_size + section_alignment - 1) & ~(section_alignment - 1));
        reloc_raw_size = (uint32_t)((reloc_data_size + file_alignment - 1) & ~(file_alignment - 1));
        image_size += reloc_virtual_size;
    }
    
    size_t total_file_size = headers_size;
    for (size_t i = 0; i < input->section_count; i++) {
        uint32_t raw_size = (uint32_t)((input->sections[i].size + file_alignment - 1) & ~(file_alignment - 1));
        total_file_size += raw_size;
    }
    if (has_idata_section) {
        total_file_size += idata_raw_size;
    }
    if (has_edata_section) {
        total_file_size += edata_raw_size;
    }
    if (has_reloc_section) {
        total_file_size += reloc_raw_size;
    }

    output->data = (uint8_t*)calloc(1, total_file_size);
    if (!output->data) {
        free(temp_maps);
        if (reloc_data) free(reloc_data);
        if (import_data) free(import_data);
        if (export_data) free(export_data);
        return ARK_LINK_ERR_MEMORY;
    }

    output->size = total_file_size;
    output->image_base = input->image_base ? input->image_base : 0x140000000;

    output->section_maps = (ArkSectionRvaMap*)calloc(total_sections, sizeof(ArkSectionRvaMap));
    if (!output->section_maps) {
        free(output->data);
        free(temp_maps);
        if (reloc_data) free(reloc_data);
        if (import_data) free(import_data);
        output->data = NULL;
        return ARK_LINK_ERR_MEMORY;
    }

    output->section_count = total_sections;

    write_dos_header(output->data);

    uint32_t entry_point_rva = section_alignment;
    if (input->entry_section > 0 && input->entry_section <= input->section_count) {
        entry_point_rva = temp_maps[input->entry_section - 1].rva + input->entry_offset;
    }

    if (has_idata_section) {
        
    }

    uint32_t reloc_rva = 0;
    if (has_reloc_section) {
        reloc_rva = current_rva;
        if (has_idata_section) {
            reloc_rva = idata_rva + idata_virtual_size;
        }
    }

    uint32_t pe_offset = 0x80;
    
    uint8_t* pe_sig_ptr = output->data + pe_offset;
    *(uint32_t*)pe_sig_ptr = PE_SIGNATURE;
    
    PE_COFF_HEADER* coff = (PE_COFF_HEADER*)(output->data + pe_offset + 4);
    coff->Machine = PE_MACHINE_AMD64;
    coff->NumberOfSections = (uint16_t)total_sections;
    coff->TimeDateStamp = 0;
    coff->PointerToSymbolTable = 0;
    coff->NumberOfSymbols = 0;
    coff->SizeOfOptionalHeader = sizeof(PE_OPTIONAL_HEADER_64);
    coff->Characteristics = PE_CHAR_EXECUTABLE_IMAGE | PE_CHAR_LARGE_ADDRESS_AWARE | PE_CHAR_DEBUG_STRIPPED;

    size_t opt_offset = pe_offset + 4 + sizeof(PE_COFF_HEADER);
    if (opt_offset + sizeof(PE_OPTIONAL_HEADER_64) > total_file_size) {
        free(temp_maps);
        if (reloc_data) free(reloc_data);
        if (import_data) free(import_data);
        free(output->data);
        output->data = NULL;
        return ARK_LINK_ERR_MEMORY;
    }
    uint8_t* opt_ptr = output->data + opt_offset;
    PE_OPTIONAL_HEADER_64* opt = (PE_OPTIONAL_HEADER_64*)opt_ptr;
    opt->Magic = PE_OPT_HDR_MAGIC_PE32_PLUS;
    opt->MajorLinkerVersion = 1;
    opt->MinorLinkerVersion = 0;
    
    uint32_t size_of_code = 0;
    for (size_t i = 0; i < input->section_count; i++) {
        if (input->sections[i].kind == ARK_SECTION_CODE) {
            uint32_t aligned_size = (uint32_t)((input->sections[i].size + section_alignment - 1) & ~(section_alignment - 1));
            size_of_code += aligned_size;
        }
    }
    opt->SizeOfCode = size_of_code;
    
    uint32_t size_of_initialized_data = 0;
    for (size_t i = 0; i < input->section_count; i++) {
        if (input->sections[i].kind == ARK_SECTION_DATA) {
            uint32_t aligned_size = (uint32_t)((input->sections[i].size + section_alignment - 1) & ~(section_alignment - 1));
            size_of_initialized_data += aligned_size;
        }
    }
    
    if (has_idata_section) {
        size_of_initialized_data += idata_virtual_size;
    }
    
    if (has_edata_section) {
        size_of_initialized_data += edata_virtual_size;
    }
    opt->SizeOfInitializedData = size_of_initialized_data;
    
    opt->SizeOfUninitializedData = 0;
    opt->AddressOfEntryPoint = entry_point_rva;
    opt->BaseOfCode = section_alignment;
    opt->ImageBase = output->image_base;
    opt->SectionAlignment = section_alignment;
    opt->FileAlignment = file_alignment;
    opt->MajorOperatingSystemVersion = 6;
    opt->MinorOperatingSystemVersion = 0;
    opt->MajorImageVersion = 0;
    opt->MinorImageVersion = 0;
    opt->MajorSubsystemVersion = 6;
    opt->MinorSubsystemVersion = 0;
    opt->Win32VersionValue = 0;
    opt->SizeOfImage = image_size;
    
    size_t headers_size_calc = 0x80 + 4 + sizeof(PE_COFF_HEADER) + sizeof(PE_OPTIONAL_HEADER_64) + total_sections * 40;
    opt->SizeOfHeaders = (uint32_t)((headers_size_calc + file_alignment - 1) & ~(file_alignment - 1));
    opt->CheckSum = 0;
    opt->Subsystem = PE_SUBSYSTEM_WINDOWS_CUI;
    
    opt->DllCharacteristics = 0x0120;
    opt->SizeOfStackReserve = 0x100000;
    opt->SizeOfStackCommit = 0x1000;
    opt->SizeOfHeapReserve = 0x100000;
    opt->SizeOfHeapCommit = 0x1000;
    opt->LoaderFlags = 0;
    opt->NumberOfRvaAndSizes = 16;

    for (int i = 0; i < 16; i++) {
        opt->DataDirectory[i].VirtualAddress = 0;
        opt->DataDirectory[i].Size = 0;
    }

    if (has_edata_section) {
        opt->DataDirectory[PE_DD_EXPORT].VirtualAddress = edata_rva;
        opt->DataDirectory[PE_DD_EXPORT].Size = (uint32_t)export_data_size;
    }

    if (has_idata_section) {
        opt->DataDirectory[PE_DD_IMPORT].VirtualAddress = idata_rva;
        opt->DataDirectory[PE_DD_IMPORT].Size = (uint32_t)import_data_size;
        
        size_t actual_iat_entries = import_iat_total_entries > 0 ? import_iat_total_entries - 1 : 0;
        opt->DataDirectory[PE_DD_IAT].VirtualAddress = import_iat_rva;
        opt->DataDirectory[PE_DD_IAT].Size = (uint32_t)(actual_iat_entries * sizeof(PE_IMPORT_LOOKUP_ENTRY));
    }

    if (has_reloc_section) {
        opt->DataDirectory[PE_DD_BASE_RELOCATION].VirtualAddress = reloc_rva;
        opt->DataDirectory[PE_DD_BASE_RELOCATION].Size = (uint32_t)reloc_data_size;
    }

    current_rva = section_alignment;
    uint32_t current_file_offset = headers_size;

    for (size_t i = 0; i < input->section_count; i++) {
        ArkSectionBuffer* sec = &input->sections[i];

        uint32_t virtual_size = (uint32_t)sec->size;
        uint32_t virtual_address = current_rva;
        uint32_t raw_size = (uint32_t)((sec->size + file_alignment - 1) & ~(file_alignment - 1));
        uint32_t raw_offset = current_file_offset;

        uint32_t characteristics = PE_SCN_MEM_READ;
        if (sec->flags & ARK_SECTION_EXEC) {
            characteristics |= PE_SCN_CNT_CODE | PE_SCN_MEM_EXECUTE;
        }
        if (sec->flags & ARK_SECTION_WRITE) {
            characteristics |= PE_SCN_MEM_WRITE;
        }
        if (!(sec->flags & ARK_SECTION_EXEC) && (sec->flags & ARK_SECTION_READ)) {
            characteristics |= PE_SCN_CNT_INITIALIZED_DATA;
        }

        char section_name[9] = {0};
        if (sec->kind == 1) {
            strcpy(section_name, ".text");
        } else if (sec->kind == 2) {
            strcpy(section_name, ".data");
        } else if (sec->kind == 3) {
            strcpy(section_name, ".rdata");
        } else {
            snprintf(section_name, 8, "sect%zu", i);
        }
        write_section_header(output->data, i, section_name, virtual_size, virtual_address,
                            raw_size, raw_offset, characteristics);

        output->section_maps[i].rva = virtual_address;
        output->section_maps[i].size = virtual_size;
        output->section_maps[i].file_offset = raw_offset;
        output->section_maps[i].flags = sec->flags;

        if (sec->data && sec->size > 0) {
            memcpy(output->data + raw_offset, sec->data, sec->size);
        }

        current_rva += (virtual_size + section_alignment - 1) & ~(section_alignment - 1);
        current_file_offset += raw_size;
    }

    if (has_idata_section) {
        uint32_t idata_section_idx = (uint32_t)input->section_count;
        uint32_t idata_file_offset = current_file_offset;

        write_section_header(output->data, idata_section_idx, ".idata",
                            (uint32_t)import_data_size, idata_rva,
                            idata_raw_size, idata_file_offset,
                            PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ);

        output->section_maps[idata_section_idx].rva = idata_rva;
        output->section_maps[idata_section_idx].size = (uint32_t)import_data_size;
        output->section_maps[idata_section_idx].file_offset = idata_file_offset;
        output->section_maps[idata_section_idx].flags = ARK_SECTION_READ;

        if (import_data) {
            memcpy(output->data + idata_file_offset, import_data, import_data_size);
        }

        current_file_offset += idata_raw_size;
    }

    if (has_edata_section) {
        uint32_t edata_section_idx = (uint32_t)(input->section_count + (has_idata_section ? 1 : 0));
        uint32_t edata_file_offset = current_file_offset;

        write_section_header(output->data, edata_section_idx, ".edata",
                            (uint32_t)export_data_size, edata_rva,
                            edata_raw_size, edata_file_offset,
                            PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ);

        output->section_maps[edata_section_idx].rva = edata_rva;
        output->section_maps[edata_section_idx].size = (uint32_t)export_data_size;
        output->section_maps[edata_section_idx].file_offset = edata_file_offset;
        output->section_maps[edata_section_idx].flags = ARK_SECTION_READ;

        if (export_data) {
            memcpy(output->data + edata_file_offset, export_data, export_data_size);
        }

        current_file_offset += edata_raw_size;
    }

    if (has_reloc_section) {
        uint32_t reloc_section_idx = (uint32_t)(input->section_count + (has_idata_section ? 1 : 0) + (has_edata_section ? 1 : 0));
        uint32_t reloc_file_offset = current_file_offset;

        write_section_header(output->data, reloc_section_idx, ".reloc",
                            (uint32_t)reloc_data_size, reloc_rva,
                            reloc_raw_size, reloc_file_offset,
                            PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ);

        output->section_maps[reloc_section_idx].rva = reloc_rva;
        output->section_maps[reloc_section_idx].size = (uint32_t)reloc_data_size;
        output->section_maps[reloc_section_idx].file_offset = reloc_file_offset;
        output->section_maps[reloc_section_idx].flags = ARK_SECTION_READ;

        if (reloc_data) {
            memcpy(output->data + reloc_file_offset, reloc_data, reloc_data_size);
        }
    }

    if (input->relocs && input->reloc_count > 0) {
        for (size_t i = 0; i < input->reloc_count; i++) {
            ArkResolverReloc* reloc = &input->relocs[i];
            if (!reloc->symbol) {
                continue;
            }

            uint64_t symbol_addr;
            fprintf(stderr, "[ArkLink] Reloc %zu: symbol=%s, section_index=%u, value=0x%x, import_module=%s\n",
                    i, reloc->symbol->name ? reloc->symbol->name : "(null)",
                    reloc->symbol->section_index, reloc->symbol->value,
                    reloc->symbol->import_module ? reloc->symbol->import_module : "(null)");
            if (reloc->symbol->import_module != NULL) {
                
                uint32_t import_idx = 0;
                for (size_t j = 0; j < input->import_count; j++) {
                    if (strcmp(input->imports[j].symbol, reloc->symbol->name) == 0) {
                        import_idx = (uint32_t)j;
                        break;
                    }
                }
                symbol_addr = output->image_base + import_iat_rva + import_idx * sizeof(PE_IMPORT_LOOKUP_ENTRY);
                fprintf(stderr, "[ArkLink]   External symbol: import_idx=%u, symbol_addr=0x%llx\n", import_idx, (unsigned long long)symbol_addr);
            } else {
                
                symbol_addr = output->image_base + output->section_maps[reloc->symbol->section_index].rva + reloc->symbol->value;
                fprintf(stderr, "[ArkLink]   Local symbol: section_rva=0x%x, symbol_value=0x%x, symbol_addr=0x%llx\n",
                        output->section_maps[reloc->symbol->section_index].rva, reloc->symbol->value, (unsigned long long)symbol_addr);
            }

            uint32_t reloc_file_offset = output->section_maps[reloc->section_index].file_offset + reloc->offset;

            switch (reloc->type) {
                case ARK_RELOC_ABS64: {
                    
                    uint64_t value = symbol_addr + reloc->addend;
                    memcpy(output->data + reloc_file_offset, &value, sizeof(uint64_t));
                    break;
                }
                case ARK_RELOC_ADDR32: {
                    
                    uint32_t value = (uint32_t)(symbol_addr + reloc->addend);
                    memcpy(output->data + reloc_file_offset, &value, sizeof(uint32_t));
                    break;
                }
                case ARK_RELOC_PC32: {
                    
                    uint32_t reloc_rva = output->section_maps[reloc->section_index].rva + reloc->offset;
                    uint32_t value = (uint32_t)(symbol_addr + reloc->addend - (output->image_base + reloc_rva + 4));
                    fprintf(stderr, "[ArkLink]   PC32: reloc_rva=0x%x, addend=%d, value=0x%x\n", reloc_rva, reloc->addend, value);
                    memcpy(output->data + reloc_file_offset, &value, sizeof(uint32_t));
                    break;
                }
                default:
                    break;
            }
        }
    }

    free(temp_maps);
    if (reloc_data) free(reloc_data);
    if (import_data) free(import_data);
    if (export_data) free(export_data);

    return ARK_LINK_OK;
}