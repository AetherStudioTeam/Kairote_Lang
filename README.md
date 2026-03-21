<div align="center">

<svg width="200" height="60" viewBox="0 0 200 60" xmlns="http://www.w3.org/2000/svg" style="cursor: pointer; transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1); translate: none; rotate: none; scale: none; transform: translate(0px, 0px);"><text x="15" y="38" font-family="'Google Sans', 'Segoe UI', sans-serif" font-size="30" font-weight="1000" fill="url(#esharpGradient)" filter="url(#glow)" style="transition: 0.3s;">Kairote</text><text x="100" y="30" font-family="'Google Sans', 'Segoe UI', sans-serif" font-size="14" font-weight="400" fill="#CCCCCC" letter-spacing="1" style="transition: 0.3s;">LANGUAGE</text><g transform="translate(100, 38)" style="transition: opacity 0.3s;"><text x="18" y="1.5" font-family="monospace" font-size="8" fill="#00FF88">BY AetherStudio</text></g></svg>

# **Kairote Programming Language**

### *系统级编程语言 | 编译速度比你想的快亿点点*

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Stars](https://img.shields.io/github/stars/AetherStudioTeam/Kairote_Lang?style=for-the-badge&logo=github&color=blue)](https://github.com/AetherStudioTeam/Kairote_Lang/stargazers)
[![Forks](https://img.shields.io/github/forks/AetherStudioTeam/Kairote_Lang?style=for-the-badge&logo=github&color=orange)](https://github.com/AetherStudioTeam/Kairote_Lang/network/members)
[![Issues](https://img.shields.io/github/issues/AetherStudioTeam/Kairote_Lang?style=for-the-badge&logo=github&color=red)](https://github.com/AetherStudioTeam/Kairote_Lang/issues)
[![Last Commit](https://img.shields.io/github/last-commit/AetherStudioTeam/Kairote_Lang?style=for-the-badge&logo=git&color=green)](https://github.com/AetherStudioTeam/Kairote_Lang/commits)
[![Code Size](https://img.shields.io/github/languages/code-size/AetherStudioTeam/Kairote_Lang?style=for-the-badge&logo=github&color=purple)](https://github.com/AetherStudioTeam/Kairote_Lang)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Python](https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white)](https://www.python.org/)
[![Made by Aether](https://img.shields.io/badge/Made%20by-Aether%20Studio-00d4aa?style=for-the-badge)](https://www.aetstudio.xyz)

[官网](https://es.aetstudio.xyz/) | [文档](https://es.aetstudio.xyz/docs) | [讨论区](https://github.com/AetherStudioTeam/Kairote_Lang/discussions) | [提交问题](https://github.com/AetherStudioTeam/Kairote_Lang/issues/new/choose)

</div>

---

## 概述

**Kairote** 是一个由 **Aether Studio** 开发的现代系统级编程语言，专为追求极致性能与优雅语法的开发者而生。

> **我们的愿景**：打造一款兼具 C++ 性能(但目前仅在部分领域上赶超)、C# 优雅、Rust 安全(当前阶段尚未实现)的编程语言，让系统编程变得简单而强大。

### 为什么选择 Kairote？

| 特性 | 描述 |
|------|------|
| **极致性能** | 自研编译器 toolchain，零成本抽象，编译速度极快 |
| **优雅语法** | 基于 C# 语法扩展，学习曲线平缓，表达能力强大 |
| **自研工具链** | 独立实现的前端、SSA 形式的 IR 及多目标后端代码生成 |
| **现代架构** | 模块化设计，支持增量编译和并行构建 |
| **类型安全** | 静态类型系统，编译期捕获错误 |

---

## 预览

```csharp
// Kairote 代码示例
module HelloWorld;

import System.Console;

public class Program
{
    public static void Main(string[] args)
    {
        // 多元运算符 - Kairote 独有特性
        var result = a <> b <> c;  // 范围比较
        
        Console.WriteLine($"Hello, Kairote World!");
    }
}
```

---

## 快速开始

### 环境要求

- **操作系统**: Windows 10/11 | Linux | macOS
- **Python**: 3.8+
- **编译器**: GCC 或 MSVC

### 安装

```bash
# 克隆仓库
git clone https://github.com/AetherStudioTeam/Kairote_Lang.git
cd Kairote_Lang

# 安装依赖
pip install -r requirements.txt
```

### 构建项目

```bash
# 开发构建
python build.py

# 发布构建（优化）
python build.py --release
```

### 编写你的第一个 Kairote 程序 或 [学习文档](./docs/doc/README.md)

创建 `hello.es` 文件：

```csharp
module Hello;

public class Program
{
    public static void Main()
    {
        System.Console.WriteLine("Hello, Kairote!");
    }
}
```

编译并运行：

```bash
./esc build hello.es
./hello
```

---

## 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                        Kairote Toolchain                    │
├─────────────┬─────────────┬─────────────┬───────────────────┤
│   ESC       │   ArkLink   │    LSP      │   Runtime         │
│  编译器      │   链接器     │  语言服务    │   运行时           │
├─────────────┼─────────────┼─────────────┼───────────────────┤
│ • 词法分析   │ • COFF/ELF  │ • 代码补全   │ • 内存管理         │
│ • 语法分析   │ • PE 后端   │ • 错误诊断   │ • 垃圾回收         │
│ • 语义分析   │ • 符号解析   │ • 跳转定义   │ • 异常处理         │
│ • SSA IR    │ • 重定位     │ • 悬停提示   │ • 标准库           │
│ • 代码生成   │ • 库文件     │             │                   │
└─────────────┴─────────────┴─────────────┴───────────────────┘
```

### 项目结构

```
Kairote_Lang/
├── ESC/                    # 编译器核心
│   ├── src/
│   │   ├── compiler/       # 编译器前端、中端、后端
│   │   ├── runtime/        # 运行时库
│   │   └── tools/          # 开发工具
│   └── build.py            # 构建脚本
├── ArkLink/                # 链接器
│   ├── src/
│   │   ├── core/           # 核心链接逻辑
│   │   ├── backends/       # 目标文件格式后端
│   │   └── cli/            # 命令行接口
│   └── include/            # 头文件
├── lsp/                    # 语言服务器协议实现
└── docs/                   # 文档
```

---

## 技术特性

### 编译器 (ESC)

- **前端**: 递归下降语法分析器，支持完整的 C# 语法子集
- **中端**: SSA 形式的中间表示，支持多种优化 passes
- **后端**: 多目标代码生成（x86, x64, ARM）

### 链接器 (ArkLink)

- 支持 COFF、ELF、PE 格式
- 增量链接支持
- 符号版本控制
- 死代码消除

### 语言服务器 (LSP)

- 实时代码诊断
- 智能代码补全
- 符号导航
- 重构支持

---

## 路线图

- [x] 基础编译器框架
- [x] 链接器实现
- [x] LSP 基础功能
- [ ] 标准库完善
- [ ] 包管理器
- [ ] IDE 插件
- [ ] JIT 编译支持
- [ ] 跨平台优化

---

## 参与贡献

我们欢迎所有形式的贡献！无论是提交 Bug 报告、功能建议，还是代码贡献。

### 如何贡献

1. **Fork** 本仓库
2. 创建你的 **Feature Branch** (`git checkout -b feature/AmazingFeature`)
3. **Commit** 你的更改 (`git commit -m 'Add some AmazingFeature'`)
4. **Push** 到分支 (`git push origin feature/AmazingFeature`)
5. 打开一个 **Pull Request**

### 提交规范

- 使用清晰的提交信息描述变更
- 确保代码通过现有测试
- 新增功能请附带测试用例
- 遵循现有代码风格

[查看贡献指南](./DevStand.md)

[提交 Bug 报告](https://github.com/AetherStudioTeam/Kairote_Lang/issues/new?template=bug_report.md)

[提交功能建议](https://github.com/AetherStudioTeam/Kairote_Lang/issues/new?template=feature_request.md)

---

## 许可证

本项目采用 [MIT License](LICENSE) 开源。

```
MIT License

Copyright (c) 2026 Aether Studio

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

---

## 相关项目

- [LumenCode](https://github.com/AetherStudioTeam/LumenCode) - 非商业音游项目
- [RedstLauncher](https://github.com/AetherStudioTeam/RedstLauncher) - Minecraft 启动器

---

## 联系我们

<div align="center">

| 渠道 | 链接 |
|------|------|
| 邮箱 | [AetherStudio@qq.com](mailto:AetherStudio@qq.com) |
| QQ 群 | [791809691](https://jq.qq.com/?_wv=1027&k=xxxxxx) |
| 官网 | [https://es.aetstudio.xyz/](https://es.aetstudio.xyz/) |
| GitHub | [https://github.com/AetherStudioTeam/Kairote_Lang](https://github.com/AetherStudioTeam/Kairote_Lang) |

</div>

---

## 致谢

### 技术参考

- [LLVM](https://llvm.org/) - SSA 设计理念
- [C# Language Specification](https://docs.microsoft.com/en-us/dotnet/csharp/) - 语法基础
- [GCC](https://gcc.gnu.org/) - 代码生成参考
- [Rust](https://www.rust-lang.org/) - 类型系统灵感

### 贡献者

感谢所有为这个项目做出贡献的开发者！

<a href="https://github.com/AetherStudioTeam/Kairote_Lang/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=AetherStudioTeam/Kairote_Lang" />
</a>

---

<div align="center">

### 如果这个项目对你有帮助，请给我们一颗星！

**Made with ❤️ by Aether Studio**

</div>
