#!/bin/bash
# build_min.sh - 从 .krt 编译到可执行文件的最小工具链
# 用法: ./build_min.sh <file.krt> [exe_name]
# 依赖: KrtC 编译器（已构建于 ./bin/KrtC），gcc

set -e

KRT_FILE="${1:-test_min.krt}"
EXE_NAME="${2:-${KRT_FILE%.krt}_exe}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# 文件名（去扩展名）
BASE="${KRT_FILE%.krt}"
ASM_FILE="${BASE}.asm"
S_FILE="${BASE}.S"
O_FILE="${BASE}.o"

# 清理旧的临时文件（保留 crt0_min.o 和 runtime.o）
rm -f "$ASM_FILE" "$S_FILE" "$O_FILE" "$EXE_NAME" output.asm

# 确保 crt0_min.o 存在
if [ ! -f crt0_min.o ]; then
    echo "=== 编译 crt0_min.c ==="
    gcc -c -o crt0_min.o crt0_min.c
fi

# 编译运行时库（如果未编译或源码更新）
RUNTIME_OBJS="runtime.o krt_memory.o krt_printf.o krt_string.o krt_misc.o allocator.o"
RUNTIME_SOURCES="src/Runtime/Runtime.c src/Runtime/Standalone/KrtMemory.c src/Runtime/Standalone/KrtPrintf.c src/Runtime/Standalone/KrtString.c src/Runtime/Standalone/KrtMisc.c src/Core/Memory/Allocator.c"
RUNTIME_INCLUDES="-Isrc -IShared -I../ArkLink/include"
RUNTIME_FLAGS="-D_GNU_SOURCE -D_DEFAULT_SOURCE"

need_runtime=0
for obj in $RUNTIME_OBJS; do
    if [ ! -f "$obj" ]; then
        need_runtime=1
        break
    fi
done

if [ "$need_runtime" -eq 1 ]; then
    echo "=== 编译运行时库 ==="
    gcc -c -o runtime.o            $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Runtime/Runtime.c
    gcc -c -o krt_memory.o         $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Runtime/Standalone/KrtMemory.c
    gcc -c -o krt_printf.o         $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Runtime/Standalone/KrtPrintf.c
    gcc -c -o krt_string.o         $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Runtime/Standalone/KrtString.c
    gcc -c -o krt_misc.o           $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Runtime/Standalone/KrtMisc.c
    gcc -c -o allocator.o          $RUNTIME_INCLUDES $RUNTIME_FLAGS src/Core/Memory/Allocator.c
    echo "  ✓ 运行时库编译完成"
fi

echo "=== KrtC 编译 .krt → .asm ==="
./bin/KrtC target asm "$KRT_FILE"

if [ ! -f output.asm ]; then
    echo "❌ 编译失败：未生成 output.asm"
    exit 1
fi
mv output.asm "$ASM_FILE"
echo "  ✓ 生成 $ASM_FILE"

echo "=== NASM → GAS Intel 转换 ==="
python3 - "$ASM_FILE" "$S_FILE" << 'PYEOF'
import sys, re
src, dst = sys.argv[1], sys.argv[2]
with open(src) as f:
    text = f.read()

out = ['.intel_syntax noprefix']
current_section = None
in_text = False

for line in text.splitlines():
    s = line.strip()
    # 跳过空行
    if not s:
        out.append('')
        continue
    # 段切换
    if s.startswith('section .'):
        sec = s.split()[1]
        if sec == '.text':
            in_text = True
            out.append('.section .text')
        else:
            in_text = False
            # 处理 .rodata / .data 中的 db 等
            out.append(f'.section {sec}')
        continue
    # 注释 - 直接跳过
    if s.startswith(';'):
        continue
    # global / extern
    if s.startswith('global '):
        name = s[7:].strip()
        # KrtC asm 把 main 函数编译为 _ZN4mainEv (C++ 名字修饰)
        # 但声明的是 global main - 需要同时声明 _ZN4mainEv
        out.append('.global ' + name)
        if name == 'main':
            out.append('.global _ZN4mainEv')
        continue
    if s.startswith('extern '):
        continue  # GAS 隐式处理
    # NASM 中 `mov reg, label` 是加载地址；GAS Intel 语法中它是加载内存内容。
    # 统一转换为 RIP 相对 LEA 以加载标签地址。
    mov_label_re = re.compile(r'^mov\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*$')
    registers = {'rax','rbx','rcx','rdx','rsi','rdi','rbp','rsp',
                 'r8','r9','r10','r11','r12','r13','r14','r15',
                 'eax','ebx','ecx','edx','esi','edi','ebp','esp',
                 'ax','bx','cx','dx','si','di','bp','sp',
                 'al','bl','cl','dl','ah','bh','ch','dh'}
    m = mov_label_re.match(s)
    if m:
        reg, label = m.group(1), m.group(2)
        if label not in registers:
            s = f'lea {reg}, [rip + {label}]'
    # 其他行：替换 db/dw/dd/dq
    s2 = re.sub(r'\bdb\b', '.byte', s)
    s2 = re.sub(r'\bdw\b', '.short', s2)
    s2 = re.sub(r'\bdd\b', '.long', s2)
    s2 = re.sub(r'\bdq\b', '.quad', s2)
    out.append(s2)

with open(dst, 'w') as f:
    f.write('\n'.join(out) + '\n')
PYEOF
echo "  ✓ 生成 $S_FILE"

echo "=== gcc 汇编 ==="
gcc -c -masm=intel -o "$O_FILE" "$S_FILE"
echo "  ✓ 生成 $O_FILE"

echo "=== gcc 链接 ==="
gcc -nostartfiles -no-pie -o "$EXE_NAME" crt0_min.o "$O_FILE" $RUNTIME_OBJS
echo "  ✓ 生成 $EXE_NAME"

echo ""
echo "=== 文件信息 ==="
file "$EXE_NAME"
ls -la "$EXE_NAME"

echo ""
echo "=== 运行 ==="
set +e
"./$EXE_NAME"
rc=$?
set -e
echo "退出码: $rc"

# 如果程序因信号崩溃（rc >= 128），让 build_min.sh 也以该码退出
if [ $rc -ge 128 ]; then
    exit $rc
fi
