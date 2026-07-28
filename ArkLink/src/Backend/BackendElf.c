#include "ArkLink/backend_elf.h"
#include "ArkLink/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#pragma pack(push, 1)

/* ELF64 File Header */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

/* ELF64 Program Header */
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

/* ELF64 Section Header */
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

/* ELF64 Symbol Table Entry */
typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

/* ELF64 RELA Relocation */
typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

/* ELF64 Dynamic Table Entry */
typedef struct {
    int64_t  d_tag;
    uint64_t d_val;
} Elf64_Dyn;

#pragma pack(pop)

/* ---------- ELF constants ---------- */
#define ELFCLASS64     2
#define ELFDATA2LSB    1
#define EV_CURRENT     1
#define ELFOSABI_NONE  0
#define ET_EXEC        2
#define ET_DYN         3
#define EM_X86_64      0x3E

#define PT_LOAD        1
#define PT_DYNAMIC     2
#define PT_INTERP      3
#define PT_TLS         7
#define PF_X           1
#define PF_W           2
#define PF_R           4

#define DT_NULL        0
#define DT_NEEDED      1
#define DT_STRTAB      5
#define DT_SYMTAB      6
#define DT_STRSZ       10
#define DT_SYMENT      11
#define DT_DEBUG       21
#define DT_INIT_ARRAY  25
#define DT_FINI_ARRAY  26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28

#define SHT_NULL       0
#define SHT_PROGBITS   1
#define SHT_SYMTAB     2
#define SHT_STRTAB     3
#define SHT_RELA       4
#define SHT_DYNAMIC    6
#define SHT_NOBITS     8
#define SHT_DYNSYM     11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_TLS        17

#define SHF_WRITE      0x1
#define SHF_ALLOC      0x2
#define SHF_EXECINSTR  0x4
#define SHF_TLS        0x400

#define STB_LOCAL      0
#define STB_GLOBAL     1
#define STB_WEAK       2
#define STT_NOTYPE     0
#define STT_OBJECT     1
#define STT_FUNC       2
#define STT_SECTION    3
#define STT_FILE       4

#define SHN_UNDEF      0
#define SHN_ABS        0xfff1

#define R_X86_64_64      1
#define R_X86_64_PC32    2
#define R_X86_64_32      10
#define R_X86_64_GOTPC32 29

#define ELF_ST_BIND(i)   ((uint8_t)((i) >> 4))
#define ELF_ST_TYPE(i)   ((uint8_t)((i) & 0xf))
#define ELF_ST_INFO(b,t) ((uint8_t)(((b) << 4) | ((t) & 0xf)))
#define ELF_R_SYM(i)     ((uint32_t)((i) >> 32))
#define ELF_R_TYPE(i)    ((uint32_t)((i) & 0xffffffff))
#define ELF_R_INFO(s,t)  (((uint64_t)(s) << 32) | (uint64_t)(t))

/* ---------- String builder (for shstrtab / strtab) ---------- */
typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
} ElfStrBuilder;

static int sb_init(ElfStrBuilder* sb) {
    sb->data = (uint8_t*)malloc(64);
    if (!sb->data) return 0;
    sb->data[0] = 0;
    sb->size = 1;
    sb->capacity = 64;
    return 1;
}

static void sb_free(ElfStrBuilder* sb) {
    free(sb->data);
    sb->data = NULL;
    sb->size = sb->capacity = 0;
}

static uint32_t sb_add(ElfStrBuilder* sb, const char* s) {
    size_t len = strlen(s) + 1;
    if (sb->size + len > sb->capacity) {
        size_t new_cap = sb->capacity;
        while (new_cap < sb->size + len) new_cap *= 2;
        uint8_t* p = (uint8_t*)realloc(sb->data, new_cap);
        if (!p) return (uint32_t)-1;
        sb->data = p;
        sb->capacity = new_cap;
    }
    uint32_t off = (uint32_t)sb->size;
    memcpy(sb->data + sb->size, s, len);
    sb->size += len;
    return off;
}

/* ---------- Section name synthesis ---------- */
static int is_tls_kind(ArkSectionKind kind);  /* forward decl (used by kind_to_sh_flags) */

static const char* kind_to_canonical(ArkSectionKind kind) {
    switch (kind) {
        case ARK_SECTION_CODE:   return ".text";
        case ARK_SECTION_DATA:   return ".data";
        case ARK_SECTION_RODATA: return ".rodata";
        case ARK_SECTION_BSS:    return ".bss";
        case ARK_SECTION_TDATA:  return ".tdata";
        case ARK_SECTION_TBSS:   return ".tbss";
        default:                 return ".data";
    }
}

/* All our user-visible section types are PROGBITS in the file. NOBITS sections
 * (.bss, .tbss) are detected separately via is_bss_kind(). SHT_TLS (17) shares
 * its numeric value with SHT_GROUP (17), which confuses binutils/readelf; the
 * convention used by gcc/ld is to keep sh_type = SHT_PROGBITS and rely on
 * SHF_TLS flag + PT_TLS program header to mark TLS sections. */
static uint32_t kind_to_sh_type(void) {
    return SHT_PROGBITS;
}

static uint64_t kind_to_sh_flags(ArkSectionKind kind, uint32_t input_flags) {
    uint64_t f = SHF_ALLOC;
    if (kind == ARK_SECTION_CODE)   f |= SHF_EXECINSTR;
    if (kind == ARK_SECTION_BSS)    f |= SHF_WRITE;
    if (kind == ARK_SECTION_DATA)   f |= SHF_WRITE;
    if (is_tls_kind(kind))          f |= SHF_WRITE | SHF_TLS;
    if (input_flags & ARK_SECTION_EXEC)  f |= SHF_EXECINSTR;
    if (input_flags & ARK_SECTION_WRITE) f |= SHF_WRITE;
    return f;
}

static int is_bss_kind(ArkSectionKind kind) {
    return kind == ARK_SECTION_BSS || kind == ARK_SECTION_TBSS;
}

static int is_tls_kind(ArkSectionKind kind) {
    return kind == ARK_SECTION_TDATA || kind == ARK_SECTION_TBSS;
}

/* pick a unique section name for the (kind, occurrence) tuple */
static uint32_t get_unique_name(ElfStrBuilder* shstrtab, ArkSectionKind kind, size_t occurrence) {
    char buf[64];
    if (occurrence == 0) {
        return sb_add(shstrtab, kind_to_canonical(kind));
    }
    snprintf(buf, sizeof(buf), "%s.%zu", kind_to_canonical(kind), occurrence);
    return sb_add(shstrtab, buf);
}

static uint32_t get_rela_name(ElfStrBuilder* shstrtab, const char* target_name) {
    char buf[80];
    snprintf(buf, sizeof(buf), ".rela%s", target_name);
    return sb_add(shstrtab, buf);
}

/* ---------- Little-endian writer ---------- */
static void write_u32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static void write_u64(uint8_t* p, uint64_t v) { memcpy(p, &v, 8); }

/* ---------- Simple (name -> symbol index) dedup map ---------- */
typedef struct {
    const char** names;
    uint32_t*    indices;
    size_t       count;
    size_t       capacity;
} NameIndexMap;

static void nim_free(NameIndexMap* m) {
    free(m->names);
    free(m->indices);
    m->names = NULL;
    m->indices = NULL;
    m->count = m->capacity = 0;
}

static int nim_lookup(NameIndexMap* m, const char* name, uint32_t* out_idx) {
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->names[i], name) == 0) {
            if (out_idx) *out_idx = m->indices[i];
            return 1;
        }
    }
    return 0;
}

static int nim_add(NameIndexMap* m, const char* name, uint32_t idx) {
    if (nim_lookup(m, name, &idx)) return 1;  /* already present */
    if (m->count == m->capacity) {
        size_t new_cap = m->capacity ? m->capacity * 2 : 16;
        const char** nn = (const char**)realloc(m->names, new_cap * sizeof(const char*));
        uint32_t* ni = (uint32_t*)realloc(m->indices, new_cap * sizeof(uint32_t));
        if (!nn || !ni) {
            free(nn); free(ni);
            return 0;
        }
        m->names = nn;
        m->indices = ni;
        m->capacity = new_cap;
    }
    m->names[m->count] = name;
    m->indices[m->count] = idx;
    m->count++;
    return 1;
}

/* ---------- Apply one relocation in-place to a section's data buffer ----------
 *
 * x86_64 ABI semantics used here:
 *   R_X86_64_64  (1)  : S + A           (64-bit absolute)
 *   R_X86_64_PC32(2)  : S + A - P       (32-bit PC-relative)
 *   R_X86_64_32  (10) : S + A           (32-bit absolute, truncated)
 *   R_X86_64_GOTPC32(29): S + A - P     (no real GOT in static link; treat as PC32)
 *   R_X86_64_SECREL(35): S + A          (section-relative: S is the symbol's offset
 *                                         within its section, NOT the final vaddr)
 *
 * For SECREL the caller must pass s_section_offset = sym->value (NOT vaddr).
 */
static void apply_reloc(uint8_t* sec_data,
                        size_t   sec_size,
                        uint32_t offset,
                        uint32_t ark_type,
                        uint64_t s_value,    /* symbol resolved address (or section offset for SECREL) */
                        uint64_t p_vaddr,    /* vaddr of the reloc location */
                        int64_t  addend) {
    int r_type = 0;
    switch (ark_type) {
        case 1: r_type = R_X86_64_64;     break;  /* ARK_RELOC_ABS64     */
        case 2: r_type = R_X86_64_32;     break;  /* ARK_RELOC_ADDR32    */
        case 3: r_type = R_X86_64_PC32;   break;  /* ARK_RELOC_PC32      */
        case 4: r_type = R_X86_64_GOTPC32; break; /* ARK_RELOC_GOTPC32   */
        case 5: r_type = R_X86_64_32;     break;  /* ARK_RELOC_SECREL32: see comment below */
        default: return;
    }

    if (r_type == R_X86_64_64) {
        if (offset + 8 > sec_size) return;
        write_u64(sec_data + offset, s_value + (uint64_t)addend);
    } else {
        if (offset + 4 > sec_size) return;
        uint64_t result;
        if (r_type == R_X86_64_PC32 || r_type == R_X86_64_GOTPC32) {
            /* PC-relative relocations are measured from the instruction boundary
             * after the relocated field (p_vaddr + 4 for a 32-bit field). */
            result = s_value + (uint64_t)addend - (p_vaddr + 4);
        } else {
            /* R_X86_64_32: caller decides s_value meaning.
             * For ABS32/SECREL32 the caller passes the section-relative offset,
             * NOT the final vaddr, matching COFF SECREL semantics. */
            result = s_value + (uint64_t)addend;
        }
        write_u32(sec_data + offset, (uint32_t)result);
    }
}

/* ============================================================
 *  Main ELF64 linker backend
 * ============================================================ */
ArkLinkResult ark_backend_elf_link(ArkLinkContext* ctx, ArkBackendInput* input, ArkBackendOutput* output) {
    (void)ctx;
    if (!input || !output) return ARK_LINK_ERR_INVALID_ARGUMENT;
    memset(output, 0, sizeof(ArkBackendOutput));

    const uint64_t page_size    = 0x1000;
    const uint64_t default_base = 0x400000;
    uint64_t image_base = input->image_base ? input->image_base : default_base;
    size_t ns = input->section_count;

    /* ---------- 1. Count relocations per input section ---------- */
    size_t* relocs_per_sec = (size_t*)calloc(ns ? ns : 1, sizeof(size_t));
    if (!relocs_per_sec) return ARK_LINK_ERR_MEMORY;
    for (size_t i = 0; i < input->reloc_count; i++) {
        uint32_t s = input->relocs[i].section_index;
        if (s < ns) relocs_per_sec[s]++;
    }
    size_t rela_sections = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] > 0) rela_sections++;
    }

    /* ---------- 2. Plan ELF section layout ---------- */
    /* [0] SHT_NULL
     * [1 .. 1+ns-1]  one ELF section per input section
     * [1+ns .. 1+ns+rela_sections-1]  one .rela.<sec> per section that has relocs
     * [symtab_idx]    .symtab
     * [strtab_idx]    .strtab
     * [shstrtab_idx]  .shstrtab
     * (if has_dynamic) +6 more: .interp, .dynsym, .dynstr, .dynamic, .init_array, .fini_array
     */
    int has_dynamic = (input->import_count > 0) ? 1 : 0;
    int has_tls = 0;
    for (size_t i = 0; i < ns; i++) {
        if (is_tls_kind((ArkSectionKind)input->sections[i].kind)) { has_tls = 1; break; }
    }

    /* Compute actual .dynstr size up front: empty string + each unique symbol + each unique module.
     * This avoids an under-estimate that would overflow into the section header table when an
     * individual name exceeds the per-import 512-byte upper bound. */
    uint64_t dynstr_actual_size = 1;   /* empty string */
    if (has_dynamic) {
        /* Walk imports and add up unique symbol + module name lengths.
         * Compare by string value (strcmp) rather than pointer to be robust against callers
         * who build import tables with strdup/copies. */
        const char** seen_names = (const char**)calloc(input->import_count * 2 + 2, sizeof(const char*));
        size_t seen_count = 0;
        for (size_t i = 0; i < input->import_count; i++) {
            const char* sym = input->imports[i].symbol;
            const char* mod = input->imports[i].module;
            int sym_seen = 0, mod_seen = 0;
            for (size_t k = 0; k < seen_count; k++) {
                if (!sym_seen && seen_names[k] && strcmp(seen_names[k], sym) == 0) sym_seen = 1;
                if (!mod_seen && seen_names[k] && strcmp(seen_names[k], mod) == 0) mod_seen = 1;
            }
            if (sym && !sym_seen) {
                dynstr_actual_size += (uint64_t)strlen(sym) + 1;
                seen_names[seen_count++] = sym;
            }
            if (mod && !mod_seen) {
                dynstr_actual_size += (uint64_t)strlen(mod) + 1;
                seen_names[seen_count++] = mod;
            }
        }
        free(seen_names);
    }
    const size_t dyn_extra = (size_t)(has_dynamic ? 6 : 0);
    size_t elf_shnum        = 1 + ns + rela_sections + 3 + dyn_extra;
    size_t rela_shidx_base  = 1 + ns;
    size_t symtab_shidx     = rela_shidx_base + rela_sections;
    size_t strtab_shidx     = symtab_shidx + 1;
    size_t shstrtab_shidx   = strtab_shidx + 1;
    size_t interp_shidx     = has_dynamic ? (shstrtab_shidx + 1) : 0;
    size_t dynsym_shidx     = has_dynamic ? (shstrtab_shidx + 2) : 0;
    size_t dynstr_shidx     = has_dynamic ? (shstrtab_shidx + 3) : 0;
    size_t dynamic_shidx    = has_dynamic ? (shstrtab_shidx + 4) : 0;
    size_t init_array_shidx = has_dynamic ? (shstrtab_shidx + 5) : 0;
    size_t fini_array_shidx = has_dynamic ? (shstrtab_shidx + 6) : 0;

    /* ---------- 3. Build .shstrtab and assign section name offsets ---------- */
    ElfStrBuilder shstrtab = {0};
    if (!sb_init(&shstrtab)) { free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    uint32_t* sec_name_off = (uint32_t*)calloc(elf_shnum, sizeof(uint32_t));
    if (!sec_name_off) { sb_free(&shstrtab); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    sec_name_off[0] = 0;
    size_t occ[6] = {0, 0, 0, 0, 0, 0};
    for (size_t i = 0; i < ns; i++) {
        ArkSectionKind k = (ArkSectionKind)input->sections[i].kind;
        size_t kidx = (k == ARK_SECTION_CODE)   ? 0 :
                      (k == ARK_SECTION_DATA)   ? 1 :
                      (k == ARK_SECTION_RODATA) ? 2 :
                      (k == ARK_SECTION_BSS)    ? 3 :
                      (k == ARK_SECTION_TDATA)  ? 4 : 5;  /* TBSS -> 5 */
        sec_name_off[1 + i] = get_unique_name(&shstrtab, k, occ[kidx]++);
    }
    size_t rela_idx = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] == 0) continue;
        const char* target_name = (const char*)shstrtab.data + sec_name_off[1 + i];
        sec_name_off[rela_shidx_base + rela_idx] = get_rela_name(&shstrtab, target_name);
        rela_idx++;
    }
    sec_name_off[symtab_shidx]   = sb_add(&shstrtab, ".symtab");
    sec_name_off[strtab_shidx]   = sb_add(&shstrtab, ".strtab");
    sec_name_off[shstrtab_shidx] = sb_add(&shstrtab, ".shstrtab");
    if (has_dynamic) {
        sec_name_off[interp_shidx]     = sb_add(&shstrtab, ".interp");
        sec_name_off[dynsym_shidx]     = sb_add(&shstrtab, ".dynsym");
        sec_name_off[dynstr_shidx]     = sb_add(&shstrtab, ".dynstr");
        sec_name_off[dynamic_shidx]    = sb_add(&shstrtab, ".dynamic");
        sec_name_off[init_array_shidx] = sb_add(&shstrtab, ".init_array");
        sec_name_off[fini_array_shidx] = sb_add(&shstrtab, ".fini_array");
    }
    if (sec_name_off[symtab_shidx] == (uint32_t)-1 || sec_name_off[strtab_shidx] == (uint32_t)-1
        || sec_name_off[shstrtab_shidx] == (uint32_t)-1) {
        sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec);
        return ARK_LINK_ERR_MEMORY;
    }

    /* ---------- 4. Build .strtab and collect symbols ---------- */
    ElfStrBuilder strtab = {0};
    if (!sb_init(&strtab)) {
        sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec);
        return ARK_LINK_ERR_MEMORY;
    }
    sb_add(&strtab, "");   /* strtab[0] = empty */
    NameIndexMap sym_map = {0};
    /* Index 0 = null symbol (no name) */
    /* Indices 1..ns = STT_SECTION locals (one per input section), unnamed (st_name=0) */
    size_t next_sym_idx = 1 + ns;

    /* helper to intern a symbol name and reserve an index */
    #define INTERN_SYM(name_ptr, out_idx) do {                                  \
        if (!nim_lookup(&sym_map, (name_ptr), (out_idx))) {                     \
            uint32_t no = sb_add(&strtab, (name_ptr));                          \
            if (no == (uint32_t)-1) goto oom;                                   \
            if (!nim_add(&sym_map, (name_ptr), (uint32_t)next_sym_idx)) goto oom;\
            *(out_idx) = (uint32_t)next_sym_idx++;                              \
        }                                                                       \
    } while (0)

    /* Add exports as globals */
    for (size_t i = 0; i < input->export_count; i++) {
        const char* n = input->exports[i].name;
        if (!n) continue;
        uint32_t idx;
        INTERN_SYM(n, &idx);
    }
    /* Add imports as globals (name = "module\0symbol" or just symbol) */
    for (size_t i = 0; i < input->import_count; i++) {
        const char* sym = input->imports[i].symbol;
        if (!sym) continue;
        uint32_t idx;
        INTERN_SYM(sym, &idx);
    }
    /* Add reloc.symbols (any symbol referenced by a reloc) */
    for (size_t i = 0; i < input->reloc_count; i++) {
        const ArkResolverSymbol* s = input->relocs[i].symbol;
        if (!s || !s->name) continue;
        uint32_t idx;
        INTERN_SYM(s->name, &idx);
    }
    #undef INTERN_SYM

    size_t total_syms = next_sym_idx;
    nim_free(&sym_map);

    /* ---------- 5. Compute file layout (offsets and vaddrs) ----------
     * Two PT_LOAD segments are produced:
     *   seg1 (R-X): image_base .. image_base+rx_end     (Ehdr+Phdrs+code+rodata+debug+Shdrs)
     *   seg2 (R-W): image_base+rw_start ..              (data, plus bss in memsz)
     */

    /* 5a. Per-section alignment */
    uint64_t* sec_align = (uint64_t*)calloc(ns ? ns : 1, sizeof(uint64_t));
    if (!sec_align) { sb_free(&strtab); sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    for (size_t i = 0; i < ns; i++) {
        uint64_t a = input->sections[i].alignment ? (uint64_t)input->sections[i].alignment : 1;
        if (a & (a - 1)) a = 1;   /* not a power of two: ignore */
        if (a < 1) a = 1;
        if (a > page_size) a = page_size;
        sec_align[i] = a;
    }

    /* 5b. Classify R-X vs R-W */
    int* is_r_w = (int*)calloc(ns ? ns : 1, sizeof(int));
    if (!is_r_w) { free(sec_align); sb_free(&strtab); sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    for (size_t i = 0; i < ns; i++) {
        ArkSectionKind k = (ArkSectionKind)input->sections[i].kind;
        is_r_w[i] = (k == ARK_SECTION_DATA || k == ARK_SECTION_BSS || is_tls_kind(k)) ? 1 : 0;
    }

    const uint64_t ehdr_size  = sizeof(Elf64_Ehdr);
    const uint64_t phdr_size  = sizeof(Elf64_Phdr);
    /* Phdrs: PT_LOAD R-X, PT_LOAD R-W, [PT_INTERP, PT_DYNAMIC if has_dynamic], [PT_TLS if has_tls] */
    const uint64_t headers_total = ehdr_size
        + (2 + 2 * (uint64_t)has_dynamic + (uint64_t)has_tls) * phdr_size;

    uint64_t* sec_offset = (uint64_t*)calloc(elf_shnum, sizeof(uint64_t));
    if (!sec_offset) { free(is_r_w); free(sec_align); sb_free(&strtab); sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    uint64_t* sec_vaddr   = (uint64_t*)calloc(elf_shnum, sizeof(uint64_t));
    if (!sec_vaddr) { free(sec_offset); free(is_r_w); free(sec_align); sb_free(&strtab); sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }
    uint64_t* sec_size    = (uint64_t*)calloc(elf_shnum, sizeof(uint64_t));
    if (!sec_size) { free(sec_vaddr); free(sec_offset); free(is_r_w); free(sec_align); sb_free(&strtab); sb_free(&shstrtab); free(sec_name_off); free(relocs_per_sec); return ARK_LINK_ERR_MEMORY; }

    #define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((uint64_t)((a) - 1)))

    /* 5c. R-X input sections */
    uint64_t cur_offset = headers_total;

    /* 5c.0. .interp lives right after the Ehdr+Phdrs (PT_INTERP must point inside seg1) */
    if (has_dynamic) {
        static const char interp_path[] = "/lib64/ld-linux-x86-64.so.2";
        size_t interp_size = sizeof(interp_path);  /* includes trailing NUL */
        sec_offset[interp_shidx] = cur_offset;
        sec_vaddr[interp_shidx]   = image_base + cur_offset;
        sec_size[interp_shidx]    = (uint64_t)interp_size;
        cur_offset += interp_size;
    }

    /* 5c. R-X input sections */
    for (size_t i = 0; i < ns; i++) {
        if (is_r_w[i]) continue;
        cur_offset = ALIGN_UP(cur_offset, sec_align[i]);
        sec_offset[1 + i] = cur_offset;
        sec_vaddr[1 + i]   = image_base + cur_offset;
        sec_size[1 + i]    = (uint64_t)input->sections[i].size;
        cur_offset += sec_size[1 + i];
    }

    /* 5d. R-X debug sections: .rela.* + .symtab + .strtab + .shstrtab */
    rela_idx = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] == 0) continue;
        cur_offset = ALIGN_UP(cur_offset, sizeof(uint64_t));
        sec_offset[rela_shidx_base + rela_idx] = cur_offset;
        sec_vaddr[rela_shidx_base + rela_idx]   = 0;
        sec_size[rela_shidx_base + rela_idx]    = (uint64_t)(relocs_per_sec[i] * sizeof(Elf64_Rela));
        cur_offset += sec_size[rela_shidx_base + rela_idx];
        rela_idx++;
    }
    cur_offset = ALIGN_UP(cur_offset, sizeof(uint64_t));
    sec_offset[symtab_shidx] = cur_offset;
    sec_vaddr[symtab_shidx]   = 0;
    sec_size[symtab_shidx]    = (uint64_t)(total_syms * sizeof(Elf64_Sym));
    cur_offset += sec_size[symtab_shidx];

    cur_offset = ALIGN_UP(cur_offset, 1);
    sec_offset[strtab_shidx] = cur_offset;
    sec_vaddr[strtab_shidx]   = 0;
    sec_size[strtab_shidx]    = (uint64_t)strtab.size;
    cur_offset += sec_size[strtab_shidx];

    cur_offset = ALIGN_UP(cur_offset, 1);
    sec_offset[shstrtab_shidx] = cur_offset;
    sec_vaddr[shstrtab_shidx]   = 0;
    sec_size[shstrtab_shidx]    = (uint64_t)shstrtab.size;
    cur_offset += sec_size[shstrtab_shidx];

    /* 5d.1. .dynsym + .dynstr (only if has_dynamic) */
    /* .dynsym: null symbol + one STB_GLOBAL STT_NOTYPE SHN_UNDEF per import */
    if (has_dynamic) {
        cur_offset = ALIGN_UP(cur_offset, sizeof(uint64_t));
        sec_offset[dynsym_shidx] = cur_offset;
        sec_vaddr[dynsym_shidx]   = image_base + cur_offset;
        sec_size[dynsym_shidx]    = (uint64_t)((1 + input->import_count) * sizeof(Elf64_Sym));
        cur_offset += sec_size[dynsym_shidx];

        cur_offset = ALIGN_UP(cur_offset, 1);
        sec_offset[dynstr_shidx] = cur_offset;
        sec_vaddr[dynstr_shidx]   = image_base + cur_offset;
        /* Use the precomputed actual size (computed in step 2 from unique symbol + module names)
         * rather than a fixed per-import upper bound. Otherwise a single long name would make
         * the real dynstr exceed this region and overflow into the section header table. */
        sec_size[dynstr_shidx]    = dynstr_actual_size;
        cur_offset += sec_size[dynstr_shidx];
    }

    /* 5e. Section header table (still in R-X) */
    cur_offset = ALIGN_UP(cur_offset, sizeof(uint64_t));
    uint64_t shdr_offset = cur_offset;
    uint64_t shdr_size   = (uint64_t)(elf_shnum * sizeof(Elf64_Shdr));
    cur_offset += shdr_size;

    /* 5f. R-X segment ends here. Pad to page boundary. */
    uint64_t rx_end = ALIGN_UP(cur_offset, page_size);
    cur_offset = rx_end;

    /* 5g. R-W input sections: .data (PROGBITS), .bss (NOBITS), .tdata (PROGBITS),
     *                        .tbss (NOBITS), and .dynamic.
     *     vaddr_off advances for BOTH PROGBITS and NOBITS (so reserved vaddr
     *     space doesn't overlap), but file_off only advances for PROGBITS. */
    uint64_t rw_start = cur_offset;
    uint64_t vaddr_off = cur_offset;
    uint64_t file_off  = cur_offset;
    /* PT_TLS aggregates: PT_TLS covers all TLS sections in the R-W segment. */
    uint64_t tls_offset = 0, tls_vaddr = 0;
    uint64_t tls_filesz = 0, tls_memsz = 0, tls_align = 1;
    int tls_started = 0;

    /* 5g.0. .dynamic (if has_dynamic) — placed in R-W since PT_DYNAMIC may be written */
    if (has_dynamic) {
        vaddr_off = ALIGN_UP(vaddr_off, sizeof(uint64_t));
        file_off  = ALIGN_UP(file_off,  sizeof(uint64_t));
        sec_offset[dynamic_shidx] = file_off;
        sec_vaddr[dynamic_shidx]   = image_base + vaddr_off;
        /* Upper bound: DT_STRTAB + DT_SYMTAB + DT_STRSZ + DT_SYMENT + N×DT_NEEDED
         *             + DT_INIT_ARRAY + DT_INIT_ARRAYSZ + DT_FINI_ARRAY + DT_FINI_ARRAYSZ + DT_NULL.
         * Use import_count as upper bound on unique modules. */
        sec_size[dynamic_shidx]    = (uint64_t)((9 + input->import_count) * sizeof(Elf64_Dyn));
        vaddr_off += sec_size[dynamic_shidx];
        file_off  += sec_size[dynamic_shidx];
    }

    /* 5g.0a. .init_array and .fini_array (size 0) — still get a vaddr inside R-W */
    if (has_dynamic) {
        vaddr_off = ALIGN_UP(vaddr_off, sizeof(uint64_t));
        file_off  = ALIGN_UP(file_off,  sizeof(uint64_t));
        sec_offset[init_array_shidx] = file_off;
        sec_vaddr[init_array_shidx]   = image_base + vaddr_off;
        sec_size[init_array_shidx]    = 0;
        sec_offset[fini_array_shidx] = file_off;
        sec_vaddr[fini_array_shidx]   = image_base + vaddr_off;
        sec_size[fini_array_shidx]    = 0;
    }

    for (size_t i = 0; i < ns; i++) {
        if (!is_r_w[i]) continue;
        vaddr_off = ALIGN_UP(vaddr_off, sec_align[i]);
        file_off  = ALIGN_UP(file_off,  sec_align[i]);
        sec_offset[1 + i] = file_off;
        sec_vaddr[1 + i]   = image_base + vaddr_off;
        sec_size[1 + i]    = (uint64_t)input->sections[i].size;
        if (is_tls_kind((ArkSectionKind)input->sections[i].kind)) {
            ArkSectionKind k = (ArkSectionKind)input->sections[i].kind;
            if (!tls_started) {
                tls_offset = file_off;
                tls_vaddr  = image_base + vaddr_off;
                tls_started = 1;
            }
            if (sec_align[i] > tls_align) tls_align = sec_align[i];
            if (k == ARK_SECTION_TDATA) {
                tls_filesz += sec_size[1 + i];
            }
            tls_memsz += sec_size[1 + i];
            /* .tdata contributes to file image; .tbss does not (NOBITS). */
            if (k == ARK_SECTION_TDATA) {
                file_off += sec_size[1 + i];
            }
            vaddr_off += sec_size[1 + i];
        } else if (is_bss_kind((ArkSectionKind)input->sections[i].kind)) {
            /* NOBITS: no file data, but vaddr space must be reserved. */
            vaddr_off += sec_size[1 + i];
        } else {
            vaddr_off += sec_size[1 + i];
            file_off  += sec_size[1 + i];
        }
    }

    /* file_size is the actual on-disk extent (driven by file_off, not vaddr_off,
     * so BSS/TBSS don't bloat the file image). */
    uint64_t file_size    = file_off;
    uint64_t rw_filesz    = file_off - rw_start;       /* data file size in segment */
    /* memsz: the R-W segment's total in-memory footprint = data + bss (incl. tbss). */
    uint64_t rw_memsz     = (vaddr_off - rw_start);
    uint64_t seg1_filesz  = rx_end;                       /* R-X covers [0, rx_end) */
    uint64_t seg1_memsz   = seg1_filesz;                  /* no BSS in R-X */
    uint64_t seg2_offset  = rw_start;
    uint64_t seg2_vaddr   = image_base + rw_start;

    #undef ALIGN_UP

    /* ---------- 6. Apply relocations to section data ---------- */
    /* s_value meaning depends on reloc type:
     *   ABS64 / ADDR32 / PC32 / GOTPC32  : s_value = sec_vaddr[1+sym_sec] + sym->value
     *   SECREL32                         : s_value = sym->value (section-relative offset)
     *   symbol with no section (import)  : s_value = 0
     */
    for (size_t i = 0; i < input->reloc_count; i++) {
        const ArkResolverReloc* r = &input->relocs[i];
        if (r->section_index >= ns) continue;
        ArkSectionBuffer* target = &input->sections[r->section_index];
        if (!target->data) continue;
        if (is_bss_kind((ArkSectionKind)target->kind)) continue;  /* BSS has no file data */
        uint64_t p_vaddr = sec_vaddr[1 + r->section_index] + r->offset;
        uint64_t s_value = 0;
        if (r->symbol) {
            if (r->symbol->section_index < ns) {
                if (r->type == 5 /* ARK_RELOC_SECREL32 */) {
                    s_value = r->symbol->value;
                } else {
                    s_value = sec_vaddr[1 + r->symbol->section_index] + r->symbol->value;
                }
            }
            /* else: undefined symbol, s_value stays 0 */
        }
        apply_reloc(target->data, target->size, r->offset, r->type, s_value, p_vaddr, r->addend);
    }

    /* ---------- 7. Build symbol table (must use same names as interned in step 4) ---------- */
    Elf64_Sym* symtab = (Elf64_Sym*)calloc(total_syms ? total_syms : 1, sizeof(Elf64_Sym));
    if (!symtab) goto oom;
    /* Section symbols (LOCAL) */
    for (size_t i = 0; i < ns; i++) {
        Elf64_Sym* s = &symtab[1 + i];
        s->st_info  = ELF_ST_INFO(STB_LOCAL, STT_SECTION);
        s->st_other = 0;
        s->st_shndx = (uint16_t)(1 + i);
        s->st_value = sec_vaddr[1 + i];
        s->st_size  = sec_size[1 + i];
    }
    /* Now add exports, imports, and reloc symbols in the same order as step 4 so indices match.
     * Note: st_name is filled in below by a second pass that rebuilds strtab deterministically;
     * here we only set the rest of the symbol metadata. */
    /* Re-derive symbol table entries by scanning the same source order. */
    next_sym_idx = 1 + ns;
    NameIndexMap sym_map2 = {0};
    for (size_t i = 0; i < input->export_count; i++) {
        const char* n = input->exports[i].name;
        if (!n) continue;
        uint32_t idx;
        if (!nim_lookup(&sym_map2, n, &idx)) {
            idx = (uint32_t)next_sym_idx++;
            Elf64_Sym* s = &symtab[idx];
            s->st_info  = ELF_ST_INFO(STB_GLOBAL, input->exports[i].is_function ? STT_FUNC : STT_OBJECT);
            s->st_other = 0;
            if (input->exports[i].section_index > 0 && input->exports[i].section_index <= ns) {
                s->st_shndx = (uint16_t)input->exports[i].section_index;
                s->st_value = sec_vaddr[input->exports[i].section_index] + input->exports[i].offset;
                s->st_size  = 0;
            } else {
                s->st_shndx = SHN_ABS;
                s->st_value = input->exports[i].value;
                s->st_size  = 0;
            }
            nim_add(&sym_map2, n, idx);
        }
    }
    for (size_t i = 0; i < input->import_count; i++) {
        const char* sym = input->imports[i].symbol;
        if (!sym) continue;
        uint32_t idx;
        if (!nim_lookup(&sym_map2, sym, &idx)) {
            idx = (uint32_t)next_sym_idx++;
            Elf64_Sym* s = &symtab[idx];
            s->st_info  = ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
            s->st_other = 0;
            s->st_shndx = SHN_UNDEF;
            s->st_value = 0;
            s->st_size  = 0;
            nim_add(&sym_map2, sym, idx);
        }
    }
    for (size_t i = 0; i < input->reloc_count; i++) {
        const ArkResolverSymbol* ss = input->relocs[i].symbol;
        if (!ss || !ss->name) continue;
        uint32_t idx;
        if (!nim_lookup(&sym_map2, ss->name, &idx)) {
            idx = (uint32_t)next_sym_idx++;
            Elf64_Sym* s = &symtab[idx];
            uint8_t bind = (ss->binding == ARK_BIND_WEAK) ? STB_WEAK : STB_GLOBAL;
            uint8_t type = STT_NOTYPE;  /* ArkResolverSymbol has no type field */
            s->st_info  = ELF_ST_INFO(bind, type);
            s->st_other = (ss->visibility == ARK_VISIBILITY_HIDDEN) ? 2 : 0;
            if (ss->section_index < ns) {
                s->st_shndx = (uint16_t)(1 + ss->section_index);
                s->st_value = sec_vaddr[1 + ss->section_index] + ss->value;
            } else {
                s->st_shndx = SHN_UNDEF;
                s->st_value = 0;
            }
            s->st_size  = ss->size;
            nim_add(&sym_map2, ss->name, idx);
        }
    }
    nim_free(&sym_map2);
    /* === Rebuild strtab deterministically and fill in st_name for globals === */
    free(strtab.data);
    sb_init(&strtab);
    sb_add(&strtab, "");
    NameIndexMap name_off_map = {0};   /* name -> strtab offset */
    size_t next_global_idx = (size_t)(1 + ns);   /* mirrors the index sequence used in step 7 */
    for (size_t i = 0; i < input->export_count; i++) {
        const char* n = input->exports[i].name;
        if (!n) continue;
        uint32_t lookup;
        if (nim_lookup(&name_off_map, n, &lookup)) { continue; }  /* already in strtab; symtab entry was set in step 7 */
        uint32_t off = sb_add(&strtab, n);
        if (off == (uint32_t)-1) goto oom;
        symtab[next_global_idx].st_name = off;
        nim_add(&name_off_map, n, off);
        next_global_idx++;
    }
    for (size_t i = 0; i < input->import_count; i++) {
        const char* sym = input->imports[i].symbol;
        if (!sym) continue;
        uint32_t lookup;
        if (nim_lookup(&name_off_map, sym, &lookup)) { continue; }  /* already handled */
        uint32_t off = sb_add(&strtab, sym);
        if (off == (uint32_t)-1) goto oom;
        symtab[next_global_idx].st_name = off;
        nim_add(&name_off_map, sym, off);
        next_global_idx++;
    }
    for (size_t i = 0; i < input->reloc_count; i++) {
        const ArkResolverSymbol* ss = input->relocs[i].symbol;
        if (!ss || !ss->name) continue;
        uint32_t lookup;
        if (nim_lookup(&name_off_map, ss->name, &lookup)) { continue; }
        uint32_t off = sb_add(&strtab, ss->name);
        if (off == (uint32_t)-1) goto oom;
        symtab[next_global_idx].st_name = off;
        nim_add(&name_off_map, ss->name, off);
        next_global_idx++;
    }
    nim_free(&name_off_map);
    /* Recompute strtab section size in case it grew */
    sec_size[strtab_shidx] = (uint64_t)strtab.size;

    /* ---------- 8. Build .rela.* section data ---------- */
    Elf64_Rela** rela_arrays = (Elf64_Rela**)calloc(rela_sections ? rela_sections : 1, sizeof(Elf64_Rela*));
    if (!rela_arrays) goto oom;
    rela_idx = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] == 0) continue;
        rela_arrays[rela_idx] = (Elf64_Rela*)calloc(relocs_per_sec[i], sizeof(Elf64_Rela));
        if (!rela_arrays[rela_idx]) { /* leak on fail is OK in this minimal impl */
            for (size_t k = 0; k < rela_idx; k++) free(rela_arrays[k]);
            free(rela_arrays);
            goto oom;
        }
        size_t cur = 0;
        for (size_t j = 0; j < input->reloc_count; j++) {
            const ArkResolverReloc* r = &input->relocs[j];
            if (r->section_index != i) continue;
            /* find the symbol's strtab name -> symtab index. We need a name->index map for globals. */
            uint32_t sym_idx = 0;
            if (r->symbol && r->symbol->name) {
                for (uint32_t k = 1 + (uint32_t)ns; k < total_syms; k++) {
                    /* search strtab for symtab[k].st_name and compare to symbol name */
                    if (symtab[k].st_name < strtab.size
                        && strcmp((const char*)strtab.data + symtab[k].st_name, r->symbol->name) == 0) {
                        sym_idx = k;
                        break;
                    }
                }
            }
            uint32_t r_type = 0;
            switch (r->type) {
                case 1: r_type = R_X86_64_64;      break;  /* ABS64     */
                case 2: r_type = R_X86_64_32;      break;  /* ADDR32    */
                case 3: r_type = R_X86_64_PC32;    break;  /* PC32      */
                case 4: r_type = R_X86_64_GOTPC32; break;  /* GOTPC32   */
                case 5: r_type = R_X86_64_32;      break;  /* SECREL32 (semantically section-relative;
                                                                  encoded as R_X86_64_32 in the .rela record
                                                                  since the reloc is already applied in step 6) */
                default: r_type = R_X86_64_64;     break;
            }
            rela_arrays[rela_idx][cur].r_offset = sec_vaddr[1 + r->section_index] + r->offset;
            rela_arrays[rela_idx][cur].r_info   = ELF_R_INFO(sym_idx, r_type);
            rela_arrays[rela_idx][cur].r_addend = r->addend;
            cur++;
        }
        rela_idx++;
    }

    /* ---------- 8.5. Build .dynsym + .dynstr + .dynamic (if has_dynamic) ---------- */
    Elf64_Sym* dynsym = NULL;
    ElfStrBuilder dynstr = {0};
    Elf64_Dyn* dyntab = NULL;
    size_t dyntab_count = 0;
    /* Free helper for rela_arrays on early-exit */
    #define FREE_RELA_ARRAYS() do { \
        for (size_t k = 0; k < rela_sections; k++) free(rela_arrays[k]); \
        free(rela_arrays); \
    } while (0)
    if (has_dynamic) {
        /* Build .dynstr: empty string + each unique import symbol name + each unique module name.
         * Track each name's offset in a map so .dynsym and DT_NEEDED can look it up O(1).
         * Also count unique modules here (since the map will already be populated by the
         * time the dynsym build runs, so checking it there would always say "duplicate"). */
        if (!sb_init(&dynstr)) { FREE_RELA_ARRAYS(); goto oom; }
        sb_add(&dynstr, "");
        NameIndexMap module_off = {0};
        NameIndexMap symbol_off = {0};
        size_t unique_modules = 0;
        for (size_t i = 0; i < input->import_count; i++) {
            const char* sym = input->imports[i].symbol;
            const char* mod = input->imports[i].module;
            if (sym && !nim_lookup(&symbol_off, sym, NULL)) {
                uint32_t off = sb_add(&dynstr, sym);
                if (off == (uint32_t)-1) {
                    nim_free(&module_off); nim_free(&symbol_off); sb_free(&dynstr);
                    FREE_RELA_ARRAYS(); goto oom;
                }
                nim_add(&symbol_off, sym, off);
            }
            if (mod && !nim_lookup(&module_off, mod, NULL)) {
                uint32_t off = sb_add(&dynstr, mod);
                if (off == (uint32_t)-1) {
                    nim_free(&module_off); nim_free(&symbol_off); sb_free(&dynstr);
                    FREE_RELA_ARRAYS(); goto oom;
                }
                nim_add(&module_off, mod, off);
                unique_modules++;
            }
        }

        /* Build .dynsym: null + one STB_GLOBAL STT_NOTYPE SHN_UNDEF per import */
        size_t dynsym_n = 1 + input->import_count;
        dynsym = (Elf64_Sym*)calloc(dynsym_n, sizeof(Elf64_Sym));
        if (!dynsym) { nim_free(&module_off); nim_free(&symbol_off); sb_free(&dynstr);
            FREE_RELA_ARRAYS(); goto oom; }
        for (size_t i = 0; i < input->import_count; i++) {
            const char* sym = input->imports[i].symbol;
            Elf64_Sym* s = &dynsym[1 + i];
            s->st_info  = ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
            s->st_other = 0;
            s->st_shndx = SHN_UNDEF;
            s->st_value = 0;
            s->st_size  = 0;
            s->st_name  = (sym && nim_lookup(&symbol_off, sym, &s->st_name)) ? s->st_name : 0;
        }

        /* Build .dynamic: DT_STRTAB, DT_SYMTAB, DT_STRSZ, DT_SYMENT,
         *                 [DT_NEEDED per module],
         *                 DT_INIT_ARRAY, DT_INIT_ARRAYSZ,
         *                 DT_FINI_ARRAY, DT_FINI_ARRAYSZ,
         *                 DT_NULL */
        dyntab_count = 4 + unique_modules + 4 + 1;   /* 4 base + N needed + 4 init/fini + 1 null */
        dyntab = (Elf64_Dyn*)calloc(dyntab_count, sizeof(Elf64_Dyn));
        if (!dyntab) { free(dynsym); nim_free(&module_off); nim_free(&symbol_off); sb_free(&dynstr);
            FREE_RELA_ARRAYS(); goto oom; }
        size_t di = 0;
        dyntab[di].d_tag = DT_STRTAB; dyntab[di].d_val = sec_vaddr[dynstr_shidx]; di++;
        dyntab[di].d_tag = DT_SYMTAB; dyntab[di].d_val = sec_vaddr[dynsym_shidx]; di++;
        dyntab[di].d_tag = DT_STRSZ;  dyntab[di].d_val = 0; di++;  /* filled in later */
        dyntab[di].d_tag = DT_SYMENT; dyntab[di].d_val = sizeof(Elf64_Sym); di++;
        /* Track which modules we've already emitted DT_NEEDED for, to avoid duplicates. */
        NameIndexMap module_emitted = {0};
        for (size_t i = 0; i < input->import_count; i++) {
            const char* mod = input->imports[i].module;
            if (!mod) continue;
            if (nim_lookup(&module_emitted, mod, NULL)) continue;   /* already emitted */
            uint32_t name_off = 0;
            if (!nim_lookup(&module_off, mod, &name_off)) continue;  /* shouldn't happen */
            dyntab[di].d_tag = DT_NEEDED;
            dyntab[di].d_val = name_off;
            di++;
            nim_add(&module_emitted, mod, 1);
        }
        nim_free(&module_emitted);
        /* DT_INIT_ARRAY / DT_INIT_ARRAYSZ — point at .init_array section */
        dyntab[di].d_tag = DT_INIT_ARRAY;    dyntab[di].d_val = sec_vaddr[init_array_shidx]; di++;
        dyntab[di].d_tag = DT_INIT_ARRAYSZ;  dyntab[di].d_val = sec_size[init_array_shidx]; di++;
        /* DT_FINI_ARRAY / DT_FINI_ARRAYSZ — point at .fini_array section */
        dyntab[di].d_tag = DT_FINI_ARRAY;    dyntab[di].d_val = sec_vaddr[fini_array_shidx]; di++;
        dyntab[di].d_tag = DT_FINI_ARRAYSZ;  dyntab[di].d_val = sec_size[fini_array_shidx]; di++;
        dyntab[di].d_tag = DT_NULL; dyntab[di].d_val = 0; di++;

        /* Now fill in real sizes and update sec_size for dynsym/dynstr/dynamic */
        sec_size[dynsym_shidx]  = (uint64_t)(dynsym_n * sizeof(Elf64_Sym));
        sec_size[dynstr_shidx]  = (uint64_t)dynstr.size;
        sec_size[dynamic_shidx] = (uint64_t)(dyntab_count * sizeof(Elf64_Dyn));
        /* patch DT_STRSZ to actual size */
        for (size_t i = 0; i < dyntab_count; i++) {
            if (dyntab[i].d_tag == DT_STRSZ) { dyntab[i].d_val = dynstr.size; break; }
        }
        nim_free(&module_off);
        nim_free(&symbol_off);
    }

    /* ---------- 9. Allocate output buffer and write everything ---------- */
    uint8_t* out_buf = (uint8_t*)calloc(1, file_size);
    if (!out_buf) {
        FREE_RELA_ARRAYS();
        if (has_dynamic) { free(dynsym); free(dyntab); sb_free(&dynstr); }
        goto oom;
    }

    /* Ehdr */
    Elf64_Ehdr ehdr = {0};
    ehdr.e_ident[0] = 0x7f;
    ehdr.e_ident[1] = 'E';
    ehdr.e_ident[2] = 'L';
    ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_NONE;
    ehdr.e_type      = ET_EXEC;  /* fixed-address executable: no ASLR; image_base is final */
    ehdr.e_machine   = EM_X86_64;
    ehdr.e_version   = EV_CURRENT;
    ehdr.e_entry     = image_base;  /* fallback if entry_section is invalid */
    /* entry_section is 1-based per Resolver; if 0 or out of range, use image_base */
    if (input->entry_section >= 1 && input->entry_section <= ns) {
        ehdr.e_entry = sec_vaddr[input->entry_section] + input->entry_offset;
    }
    ehdr.e_phoff     = sizeof(Elf64_Ehdr);
    ehdr.e_shoff     = shdr_offset;
    ehdr.e_flags     = 0;
    ehdr.e_ehsize    = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum     = (uint16_t)(2 + (has_dynamic ? 2 : 0) + (has_tls ? 1 : 0));
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum     = (uint16_t)elf_shnum;
    ehdr.e_shstrndx  = (uint16_t)shstrtab_shidx;
    memcpy(out_buf, &ehdr, sizeof(ehdr));

    /* Phdr[0]: PT_LOAD R-X (code + rodata + debug + Shdrs) */
    Elf64_Phdr phdr0 = {0};
    phdr0.p_type   = PT_LOAD;
    phdr0.p_flags  = PF_R | PF_X;
    phdr0.p_offset = 0;
    phdr0.p_vaddr  = image_base;
    phdr0.p_paddr  = image_base;
    phdr0.p_filesz = seg1_filesz;
    phdr0.p_memsz  = seg1_memsz;
    phdr0.p_align  = page_size;
    memcpy(out_buf + sizeof(Elf64_Ehdr), &phdr0, sizeof(phdr0));

    /* Phdr[1]: PT_LOAD R-W (data + bss + .dynamic + .tdata + .tbss) */
    Elf64_Phdr phdr1 = {0};
    phdr1.p_type   = PT_LOAD;
    phdr1.p_flags  = PF_R | PF_W;
    phdr1.p_offset = seg2_offset;
    phdr1.p_vaddr  = seg2_vaddr;
    phdr1.p_paddr  = seg2_vaddr;
    phdr1.p_filesz = rw_filesz;
    phdr1.p_memsz  = rw_memsz;
    phdr1.p_align  = page_size;
    memcpy(out_buf + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr), &phdr1, sizeof(phdr1));

    /* Phdr[2]: PT_INTERP (if has_dynamic)
     * Phdr[3]: PT_DYNAMIC (if has_dynamic)
     * Phdr[2 or 4]: PT_TLS (if has_tls) */
    Elf64_Phdr phdr2 = {0}, phdr3 = {0}, phdr_tls = {0};
    size_t phdr_write_idx = 2;
    if (has_dynamic) {
        phdr2.p_type   = PT_INTERP;
        phdr2.p_offset = sec_offset[interp_shidx];
        phdr2.p_vaddr  = sec_vaddr[interp_shidx];
        phdr2.p_paddr  = sec_vaddr[interp_shidx];
        phdr2.p_filesz = sec_size[interp_shidx];
        phdr2.p_memsz  = sec_size[interp_shidx];
        phdr2.p_align  = 1;
        memcpy(out_buf + sizeof(Elf64_Ehdr) + phdr_write_idx * sizeof(Elf64_Phdr), &phdr2, sizeof(phdr2));
        phdr_write_idx++;

        phdr3.p_type   = PT_DYNAMIC;
        phdr3.p_offset = sec_offset[dynamic_shidx];
        phdr3.p_vaddr  = sec_vaddr[dynamic_shidx];
        phdr3.p_paddr  = sec_vaddr[dynamic_shidx];
        phdr3.p_filesz = sec_size[dynamic_shidx];
        phdr3.p_memsz  = sec_size[dynamic_shidx];
        phdr3.p_align  = sizeof(uint64_t);
        memcpy(out_buf + sizeof(Elf64_Ehdr) + phdr_write_idx * sizeof(Elf64_Phdr), &phdr3, sizeof(phdr3));
        phdr_write_idx++;
    }
    if (has_tls) {
        phdr_tls.p_type   = PT_TLS;
        phdr_tls.p_offset = tls_offset;
        phdr_tls.p_vaddr  = tls_vaddr;
        phdr_tls.p_paddr  = tls_vaddr;
        phdr_tls.p_filesz = tls_filesz;
        phdr_tls.p_memsz  = tls_memsz;
        phdr_tls.p_flags  = PF_R | PF_W;
        phdr_tls.p_align  = tls_align;
        memcpy(out_buf + sizeof(Elf64_Ehdr) + phdr_write_idx * sizeof(Elf64_Phdr),
               &phdr_tls, sizeof(phdr_tls));
    }

    /* Section contents: input sections */
    for (size_t i = 0; i < ns; i++) {
        if (is_bss_kind((ArkSectionKind)input->sections[i].kind)) continue;
        if (input->sections[i].data && input->sections[i].size > 0) {
            memcpy(out_buf + sec_offset[1 + i], input->sections[i].data, input->sections[i].size);
        }
    }
    /* .rela.* */
    rela_idx = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] == 0) continue;
        if (rela_arrays[rela_idx]) {
            memcpy(out_buf + sec_offset[rela_shidx_base + rela_idx],
                   rela_arrays[rela_idx],
                   relocs_per_sec[i] * sizeof(Elf64_Rela));
        }
        free(rela_arrays[rela_idx]);
        rela_idx++;
    }
    free(rela_arrays);
    /* .symtab */
    memcpy(out_buf + sec_offset[symtab_shidx], symtab, total_syms * sizeof(Elf64_Sym));
    /* .strtab */
    memcpy(out_buf + sec_offset[strtab_shidx], strtab.data, strtab.size);
    /* .shstrtab */
    memcpy(out_buf + sec_offset[shstrtab_shidx], shstrtab.data, shstrtab.size);
    /* .interp, .dynsym, .dynstr, .dynamic (if has_dynamic) */
    if (has_dynamic) {
        static const char interp_path[] = "/lib64/ld-linux-x86-64.so.2";
        memcpy(out_buf + sec_offset[interp_shidx], interp_path, sizeof(interp_path));
        memcpy(out_buf + sec_offset[dynsym_shidx], dynsym, sec_size[dynsym_shidx]);
        memcpy(out_buf + sec_offset[dynstr_shidx], dynstr.data, sec_size[dynstr_shidx]);
        memcpy(out_buf + sec_offset[dynamic_shidx], dyntab, sec_size[dynamic_shidx]);
    }

    /* Section header table */
    Elf64_Shdr* shdrs = (Elf64_Shdr*)calloc(elf_shnum, sizeof(Elf64_Shdr));
    if (!shdrs) { free(out_buf); goto oom; }
    /* SHT_NULL: all zero (already) */
    /* Input sections */
    for (size_t i = 0; i < ns; i++) {
        Elf64_Shdr* sh = &shdrs[1 + i];
        sh->sh_name      = sec_name_off[1 + i];
        sh->sh_type      = is_bss_kind((ArkSectionKind)input->sections[i].kind) ? SHT_NOBITS
                                                                            : kind_to_sh_type();
        sh->sh_flags     = kind_to_sh_flags((ArkSectionKind)input->sections[i].kind, input->sections[i].flags);
        sh->sh_addr      = sec_vaddr[1 + i];
        sh->sh_offset    = sec_offset[1 + i];
        sh->sh_size      = sec_size[1 + i];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = sec_align[i];
        sh->sh_entsize   = 0;
    }
    /* .rela.* */
    rela_idx = 0;
    for (size_t i = 0; i < ns; i++) {
        if (relocs_per_sec[i] == 0) continue;
        Elf64_Shdr* sh = &shdrs[rela_shidx_base + rela_idx];
        sh->sh_name      = sec_name_off[rela_shidx_base + rela_idx];
        sh->sh_type      = SHT_RELA;
        sh->sh_flags     = 0;   /* relocations are not loaded into memory */
        sh->sh_addr      = 0;
        sh->sh_offset    = sec_offset[rela_shidx_base + rela_idx];
        sh->sh_size      = sec_size[rela_shidx_base + rela_idx];
        sh->sh_link      = (uint32_t)symtab_shidx;     /* link to .symtab */
        sh->sh_info      = (uint32_t)(1 + i);          /* info = target section index */
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(Elf64_Rela);
        rela_idx++;
    }
    /* .symtab */
    {
        Elf64_Shdr* sh = &shdrs[symtab_shidx];
        sh->sh_name      = sec_name_off[symtab_shidx];
        sh->sh_type      = SHT_SYMTAB;
        sh->sh_flags     = 0;
        sh->sh_addr      = 0;
        sh->sh_offset    = sec_offset[symtab_shidx];
        sh->sh_size      = sec_size[symtab_shidx];
        sh->sh_link      = (uint32_t)strtab_shidx;
        sh->sh_info      = (uint32_t)(1 + ns);         /* first non-local symbol index */
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(Elf64_Sym);
    }
    /* .strtab */
    {
        Elf64_Shdr* sh = &shdrs[strtab_shidx];
        sh->sh_name      = sec_name_off[strtab_shidx];
        sh->sh_type      = SHT_STRTAB;
        sh->sh_flags     = 0;
        sh->sh_addr      = 0;
        sh->sh_offset    = sec_offset[strtab_shidx];
        sh->sh_size      = sec_size[strtab_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = 1;
        sh->sh_entsize   = 0;
    }
    /* .shstrtab */
    {
        Elf64_Shdr* sh = &shdrs[shstrtab_shidx];
        sh->sh_name      = sec_name_off[shstrtab_shidx];
        sh->sh_type      = SHT_STRTAB;
        sh->sh_flags     = 0;
        sh->sh_addr      = 0;
        sh->sh_offset    = sec_offset[shstrtab_shidx];
        sh->sh_size      = sec_size[shstrtab_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = 1;
        sh->sh_entsize   = 0;
    }
    /* .interp, .dynsym, .dynstr, .dynamic (if has_dynamic) */
    if (has_dynamic) {
        Elf64_Shdr* sh;
        sh = &shdrs[interp_shidx];
        sh->sh_name      = sec_name_off[interp_shidx];
        sh->sh_type      = SHT_PROGBITS;
        sh->sh_flags     = SHF_ALLOC;
        sh->sh_addr      = sec_vaddr[interp_shidx];
        sh->sh_offset    = sec_offset[interp_shidx];
        sh->sh_size      = sec_size[interp_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = 1;
        sh->sh_entsize   = 0;

        sh = &shdrs[dynsym_shidx];
        sh->sh_name      = sec_name_off[dynsym_shidx];
        sh->sh_type      = SHT_DYNSYM;
        sh->sh_flags     = SHF_ALLOC;
        sh->sh_addr      = sec_vaddr[dynsym_shidx];
        sh->sh_offset    = sec_offset[dynsym_shidx];
        sh->sh_size      = sec_size[dynsym_shidx];
        sh->sh_link      = (uint32_t)dynstr_shidx;
        sh->sh_info      = 1;                            /* first non-local */
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(Elf64_Sym);

        sh = &shdrs[dynstr_shidx];
        sh->sh_name      = sec_name_off[dynstr_shidx];
        sh->sh_type      = SHT_STRTAB;
        sh->sh_flags     = SHF_ALLOC;
        sh->sh_addr      = sec_vaddr[dynstr_shidx];
        sh->sh_offset    = sec_offset[dynstr_shidx];
        sh->sh_size      = sec_size[dynstr_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = 1;
        sh->sh_entsize   = 0;

        sh = &shdrs[dynamic_shidx];
        sh->sh_name      = sec_name_off[dynamic_shidx];
        sh->sh_type      = SHT_DYNAMIC;
        sh->sh_flags     = SHF_ALLOC | SHF_WRITE;
        sh->sh_addr      = sec_vaddr[dynamic_shidx];
        sh->sh_offset    = sec_offset[dynamic_shidx];
        sh->sh_size      = sec_size[dynamic_shidx];
        sh->sh_link      = (uint32_t)dynstr_shidx;
        sh->sh_info      = 0;
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(Elf64_Dyn);

        /* .init_array — empty by default; dynamic linker reads DT_INIT_ARRAYSZ */
        sh = &shdrs[init_array_shidx];
        sh->sh_name      = sec_name_off[init_array_shidx];
        sh->sh_type      = SHT_INIT_ARRAY;
        sh->sh_flags     = SHF_ALLOC | SHF_WRITE;
        sh->sh_addr      = sec_vaddr[init_array_shidx];
        sh->sh_offset    = sec_offset[init_array_shidx];
        sh->sh_size      = sec_size[init_array_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(uint64_t);

        /* .fini_array — empty by default; dynamic linker reads DT_FINI_ARRAYSZ */
        sh = &shdrs[fini_array_shidx];
        sh->sh_name      = sec_name_off[fini_array_shidx];
        sh->sh_type      = SHT_FINI_ARRAY;
        sh->sh_flags     = SHF_ALLOC | SHF_WRITE;
        sh->sh_addr      = sec_vaddr[fini_array_shidx];
        sh->sh_offset    = sec_offset[fini_array_shidx];
        sh->sh_size      = sec_size[fini_array_shidx];
        sh->sh_link      = 0;
        sh->sh_info      = 0;
        sh->sh_addralign = sizeof(uint64_t);
        sh->sh_entsize   = sizeof(uint64_t);
    }
    memcpy(out_buf + shdr_offset, shdrs, elf_shnum * sizeof(Elf64_Shdr));
    free(shdrs);

    /* ---------- 10. Build output mapping (ArkSectionRvaMap) ---------- */
    if (ns > 0) {
        ArkSectionRvaMap* maps = (ArkSectionRvaMap*)calloc(ns, sizeof(ArkSectionRvaMap));
        if (!maps) { free(out_buf); goto oom; }
        for (size_t i = 0; i < ns; i++) {
            maps[i].rva        = (uint32_t)(sec_vaddr[1 + i] - image_base);
            maps[i].size       = (uint32_t)sec_size[1 + i];
            maps[i].file_offset = (uint32_t)sec_offset[1 + i];
            maps[i].flags      = input->sections[i].flags;
        }
        output->section_maps = maps;
        output->section_count = ns;
    }
    output->data        = out_buf;
    output->size        = file_size;
    output->image_base  = image_base;

    /* cleanup */
    if (has_dynamic) { free(dynsym); free(dyntab); sb_free(&dynstr); }
    free(symtab);
    free(sec_size);
    free(sec_vaddr);
    free(sec_offset);
    free(sec_align);
    free(is_r_w);
    free(sec_name_off);
    free(relocs_per_sec);
    sb_free(&strtab);
    sb_free(&shstrtab);
    return ARK_LINK_OK;

oom:
    if (has_dynamic) { free(dynsym); free(dyntab); sb_free(&dynstr); }
    free(symtab);
    free(sec_size);
    free(sec_vaddr);
    free(sec_offset);
    free(sec_align);
    free(is_r_w);
    free(sec_name_off);
    free(relocs_per_sec);
    sb_free(&strtab);
    sb_free(&shstrtab);
    return ARK_LINK_ERR_MEMORY;
}
