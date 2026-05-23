KRO (KairObject) 文件格式特性总览

设计哲学
- 最小充分性：仅保留链接器工作的最小信息集（段、符号、重定位）
- 零历史包袱：去除 DOS stub、Rich header、COFF 复杂存储类等 30 年技术债
- 专注自托管：专为 E# 编译器设计，不追求通用 COFF 兼容性
- C 友好：结构体自然对齐，无复杂位域，支持 `mmap` 零拷贝解析

---

文件结构（线性布局）

```
[KRO Header (64 bytes)]
[Section Headers (4 × 32 bytes)]  // 固定4个段
[Symbol Table (N × 40 bytes)]     // 符号数组
[String Table (连续C字符串)]       // 符号名存储
[Relocation Tables (按段分组)]     // 重定位指令
[Raw Data]                        // .text/.data/.rcontent
```

---

核心数据结构

1. 文件头（Fixed 64-byte）

```c
typedef struct {
    uint32_t magic;        // 'KRO\0' (0x004F524B)
    uint16_t version;      // 格式版本（当前为1）
    uint16_t flags;        // 保留标志位（bit0: 是否PIC）
    uint16_t arch;         // 0x8664 (x86_64), 0xAA64 (ARM64)
    uint16_t reserved;     // 对齐填充
    
    uint32_t sec_count;    // 固定为4，但保持字段以便扩展
    uint32_t sym_count;    // 符号表条目数
    
    uint64_t strtab_size;  // 字符串表总字节数
    uint64_t entry_point;  // 未链接时无效（全0）
} KROHeader;
```

2. 段头（固定4个，32-byte each）

```c
typedef struct {
    char name[8];          // ".text\0\0", ".data\0\0\0" 等
    uint8_t align_log2;    // 对齐要求：4=16字节, 12=4096字节页对齐
    uint8_t flags;         // bit0:可读 bit1:可写 bit2:可执行 bit3:零初始化(BSS)
    uint16_t reserved;     // 保留
    
    uint64_t file_offset;  // 数据在文件中的偏移（BSS为0）
    uint64_t file_size;    // 文件中占用的字节（BSS为0）
    uint64_t mem_size;     // 内存中占用的字节（BSS有效）
    uint32_t reloc_count;  // 该段的重定位条目数
    uint32_t reloc_offset; // 重定位表在文件中的偏移
} KROSection;
```

固定4段：
- `.text` (RX): 代码段，只读可执行
- `.data` (RW): 初始化数据
- `.rdata` (RO): 只读常量（字符串字面量等）
- `.bss` (RWA): 未初始化数据（文件不占空间）

3. 符号表（40-byte entry）

```c
typedef struct {
    char name[32];         // 短符号直接存储，长符号存储为 "#<hash>"
    uint64_t value;        // 段内偏移或绝对值
    uint16_t sec_idx;      // 所属段（0=未定义/外部符号）
    uint8_t  type;         // 0=NOTYPE, 1=FUNC, 2=OBJECT
    uint8_t  bind;         // 0=LOCAL, 1=GLOBAL, 2=WEAK
    uint16_t reserved;     // 对齐到40字节
} KROSymbol;
```

名称处理策略：
- ≤31 字符：直接存入 `name`（以 `\0` 结尾）
- \>31 字符：存入 `#<crc32_hash>`，完整名存入字符串表

4. 重定位条目（24-byte）

```c
typedef struct {
    uint64_t offset;       // 需修补的位置（相对于段起始）
    uint32_t sym_idx;      // 引用符号的索引（指向符号表）
    uint16_t type;         // 重定位类型（见下）
    uint16_t addend;       // 加数（用于复杂表达式，通常为0）
} KROReloc;
```

4种核心重定位类型：

类型	编码	说明	用途	
`RELOC_ABS64`	0	64位绝对地址	数据段中的全局指针	
`RELOC_PC32`	1	RIP相对32位有符号	`call/jmp` 指令	
`RELOC_GOT32`	2	GOT表相对偏移	动态链接/外部符号	
`RELOC_SECREL`	3	段内相对32位	`.rdata` 中的静态数据引用	

---

关键特性说明

内存布局优化（C实现友好）
- Arena 分配：解析单个 KRO 文件时，所有小对象（符号、重定位）从 Arena 分配，链接完成后整片释放，无碎片化
- mmap 零拷贝：通过 `mmap`（Linux）或 `CreateFileMapping`（Windows）映射文件，解析时直接指针运算，无 `memcpy`

对齐策略
- 文件内无填充：段数据紧密排列，不强制文件对齐
- 内存对齐元数据：`align_log2` 字段告诉链接器该段在虚拟内存中需要对齐到的边界（如 16 字节指令对齐、4096 字节页对齐）
- 链接器负责填充：合并段时由 ESLINKER 在段间插入 padding

明确不支持（设计决策）
- ❌ 行号信息：调试信息剥离到单独 `.pdb`/`.debug` 文件
- ❌ 复杂段属性：无 COFF 的 `IMAGE_SCN_LNK_COMDAT` 等 20+ 种标志位
- ❌ 基址重定位：对象文件阶段不处理，由链接器生成 PE/ELF 时构建 `.reloc` 段
- ❌ 导入/导出目录：KRO 是纯对象格式，导入表由 ESLINKER 链接时动态构建为 PE IDT

跨平台输出
- 输入统一：KRO 格式与平台无关（仅 `arch` 字段区分 x64/ARM64）
- 输出分化：ESLINKER 读取 KRO 后，根据目标平台输出：
  - Windows：PE32+（.exe/.dll）
  - Linux：ELF64
  - 裸机：Flat binary（无头）

---

与 COFF 对比优势

维度	COFF/PE	KRO	
文件头	多层嵌套（DOS头+PE头+COFF头+可选头=500字节）	单一64字节固定头	
段处理	可变数量，复杂属性解析	固定4段，直接索引	
符号解析	字符串表+长度前缀+辅助符号记录	固定40字节条目，短名内联	
内存占用	需多次 malloc 和字符串拷贝	mmap 零拷贝，Arena 批量释放	
链接速度	通用链接器需处理所有历史特性	ESLINKER 专注 E# 产物，O(N) 符号解析	
工具链依赖	需要完整 MSVC/MinGW 工具链	ESLINKER 单文件可执行（<500KB）	

---

版本演进预留
- `version` 字段允许未来扩展（如增加 TLS 段）
- `flags` 位掩码预留功能位（如 bit1: 是否包含 LTO 字节码）
- `reserved` 字段确保结构体对齐不变，未来可转为功能字段而不破坏 ABI

底线：KRO 是链接器输入格式，而非可执行格式。它牺牲通用性换取极致的解析速度和实现简洁性，配合 ESLINKER 实现从 E# 源码到可执行文件的闭环。