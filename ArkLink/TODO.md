# ArkLink 导入表实现进度

## 当前状态

### 已完成的工作

1. **数据结构扩展** ✅
   - `ArkImportModule` - 导入模块结构
   - `ArkImportBinding` - 导入绑定信息
   - `ArkResolverPlan` 添加导入表字段

2. **符号解析器扩展** ✅
   - 收集所有导入符号（带有 `import_module` 的符号）
   - 按 DLL 模块分组导入符号
   - 生成导入模块列表

3. **PE 结构定义** ✅
   - `PEImportDescriptor` - PE 导入表描述符
   - `PEThunk` - PE thunk 结构

### 待完成的工作

1. **生成导入表数据** ❌
   - 为每个 DLL 模块生成导入描述符
   - 生成 INT（Import Name Table）
   - 生成 IAT（Import Address Table）
   - 生成 DLL 名称字符串

2. **写入 PE 文件** ❌
   - 在 `.rdata` 或新节中写入导入表
   - 更新 Optional Header 的导入表目录项
   - 设置正确的 RVA 和大小

3. **重定位修正** ❌
   - 将对导入符号的重定位指向 IAT 条目
   - 设置正确的 thunk 索引

## 实现方案

### 方案 1: 在 PE Backend 中生成导入表

**优点**:
- 集中管理所有 PE 结构生成
- 可以精确控制 RVA 和文件偏移

**缺点**:
- 需要修改 backend 接口
- 需要传递 resolver plan 信息

### 方案 2: 在 Linker 中生成导入表（推荐）

**优点**:
- 有完整的 resolver plan 信息
- 不需要修改 backend 接口
- 更灵活

**缺点**:
- 需要在 backend 生成后手动修改 PE 数据

## 推荐实现步骤

### Step 1: 扩展 Backend Input
```c
typedef struct ArkBackendInput {
    ArkSectionBuffer* sections;
    size_t section_count;
    uint64_t entry_point;
    uint32_t entry_section;
    uint32_t entry_offset;
    
    // 新增导入表信息
    ArkImportEntry* imports;
    size_t import_count;
} ArkBackendInput;
```

### Step 2: 在 Linker 中准备导入数据
```c
// 在 link_native() 中
if (plan.import_count > 0) {
    // 为每个导入符号分配 IAT 条目
    // 设置 plan.backend_input->imports
    // 设置 plan.backend_input->import_count
}
```

### Step 3: 修改 PE Backend 生成导入表
```c
// 在 ark_backend_pe_link() 中
if (input->import_count > 0) {
    // 计算导入表大小
    // 分配 .rdata 或新节空间
    // 生成导入描述符数组
    // 生成 INT 和 IAT
    // 生成 DLL 名称和符号名称字符串
    // 更新 Optional Header 的导入表目录
}
```

### Step 4: 修正重定位
```c
// 在 apply_relocations_to_pe() 中
for (导入符号的重定位) {
    // 将重定位目标改为 IAT 条目的 RVA
    // 而不是符号的实际地址
}
```

## PE 导入表结构

```
PE 文件
├── .text (代码段)
│   └── call [IAT_Entry]  ; 调用导入函数
├── .rdata (只读数据段)
│   ├── Import Directory Table (导入目录表)
│   │   ├── Import Descriptor 1 (kernel32.dll)
│   │   ├── Import Descriptor 2 (user32.dll)
│   │   └── ...
│   ├── Import Name Table (INT)
│   │   ├── thunk_data (符号名 RVA)
│   │   └── ...
│   └── DLL Names
│       ├── "kernel32.dll\0"
│       └── "user32.dll\0"
└── .data (数据段)
    └── Import Address Table (IAT)
        ├── IAT_Entry 1 → GetProcAddress
        ├── IAT_Entry 2 → CreateFileA
        └── ...
```

## 下一步行动

1. **完成 backend.h 的扩展** - 添加导入表输入参数
2. **修改 linker_native.c** - 准备导入数据
3. **修改 backend_pe.c** - 生成导入表结构
4. **测试验证** - 创建测试程序调用 Windows API

## 参考资料

- [Microsoft PE and COFF Specification - Import Section](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-section)
- [Import Address Table (IAT)](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-address-table)
