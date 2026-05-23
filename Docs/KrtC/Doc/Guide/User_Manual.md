# Kairote Lang 用户手册

## 目录

1. [简介](#简介)
2. [安装](#安装)
3. [快速开始](#快速开始)
4. [基本语法](#基本语法)
5. [高级特性](#高级特性)
6. [标准库](#标准库)
7. [项目构建](#项目构建)
8. [常见问题](#常见问题)
9. [最佳实践](#最佳实践)

## 简介

Kairote Lang 是一种现代的、面向对象的编程语言，设计目标是提供高性能、类型安全和易用性的平衡。它融合了多种编程语言的特性，包括 C++ 的性能、C# 的语法和 Java 的跨平台能力。

### 主要特性

- **强类型系统**：提供编译时类型检查，减少运行时错误
- **面向对象编程**：支持类、继承、多态等OOP特性
- **泛型编程**：提供类型安全的泛型支持
- **内存管理**：结合垃圾回收和手动内存管理
- **异常处理**：提供健壮的错误处理机制
- **跨平台**：支持Windows、Linux等主流平台
- **高性能**：编译为本地代码，提供接近C/C++的执行效率

## 安装

### 系统要求

- 操作系统：Windows 10+ 或 Linux (Ubuntu 18.04+)
- 内存：至少 512MB 可用内存
- 磁盘空间：至少 100MB 可用空间

### Windows 安装

1. 下载 Kairote Lang 安装包（.exe文件）
2. 运行安装程序，按照提示完成安装
3. 将 Kairote Lang 安装目录添加到系统 PATH 环境变量

### Linux 安装

1. 下载 Kairote Lang 压缩包（.tar.gz文件）
2. 解压到目标目录：
   ```bash
   tar -xzf esharp.tar.gz -C /opt/
   ```
3. 创建符号链接：
   ```bash
   sudo ln -s /opt/esharp/bin/e_sharp /usr/local/bin/e_sharp
   ```

### 验证安装

打开命令行，运行以下命令：
```bash
e_sharp --version
```

如果安装成功，将显示 Kairote Lang 版本信息。

## 快速开始

### 第一个程序

创建一个名为 `hello.es` 的文件，内容如下：

```KrtL
function main() {
    print("Hello, World!");
}
```

编译并运行：
```bash
e_sharp hello.es
./hello
```

输出：
```
Hello, World!
```

### 基本程序结构

Kairote Lang 程序由一个或多个函数组成，其中必须包含一个 `main` 函数作为程序入口点：

```KrtL
function main() {
    // 程序代码
}
```

### 编译选项

基本编译命令：
```bash
e_sharp input.es                    # 编译并运行
e_sharp -c input.es                 # 只编译，不运行
e_sharp -o output.exe input.es      # 指定输出文件名
e_sharp -target x86 input.es        # 指定目标平台
e_sharp -O2 input.es                # 启用优化
e_sharp -g input.es                 # 包含调试信息
```

## 基本语法

### 变量和数据类型

#### 变量声明

```KrtL
// 类型推断
var x = 10;
var name = "Kairote Lang";
var isReady = true;

// 显式类型
int32 age = 30;
string message = "Hello";
bool flag = false;
```

#### 基本数据类型

```KrtL
// 整数类型
int8 a = 127;          // 8位有符号整数
int16 b = 32767;       // 16位有符号整数
int32 c = 2147483647;  // 32位有符号整数
int64 d = 9223372036854775807; // 64位有符号整数

// 无符号整数类型
uint8 e = 255;         // 8位无符号整数
uint16 f = 65535;      // 16位无符号整数
uint32 g = 4294967295; // 32位无符号整数
uint64 h = 18446744073709551615; // 64位无符号整数

// 浮点类型
float32 i = 3.14f;     // 32位浮点数
float64 j = 2.71828;   // 64位浮点数

// 其他类型
bool k = true;         // 布尔类型
char l = 'A';          // 字符类型
string m = "Hello";    // 字符串类型
```

#### 常量

```KrtL
const int32 MAX_SIZE = 100;
const double PI = 3.14159265359;
```

### 运算符

#### 算术运算符

```KrtL
var a = 10 + 5;    // 加法
var b = 10 - 5;    // 减法
var c = 10 * 5;    // 乘法
var d = 10 / 5;    // 除法
var e = 10 % 5;    // 取模
var f = 2 ** 3;    // 幂运算
```

#### 比较运算符

```KrtL
var a = 10 == 5;   // 等于
var b = 10 != 5;   // 不等于
var c = 10 > 5;    // 大于
var d = 10 < 5;    // 小于
var e = 10 >= 5;   // 大于等于
var f = 10 <= 5;   // 小于等于
```

#### 逻辑运算符

```KrtL
var a = true and false;  // 逻辑与
var b = true or false;   // 逻辑或
var c = not true;        // 逻辑非
```

### 控制流

#### 条件语句

```KrtL
if (x > 0) {
    print("x is positive");
} else if (x < 0) {
    print("x is negative");
} else {
    print("x is zero");
}
```

#### 循环语句

```KrtL
// while 循环
var i = 0;
while (i < 10) {
    print(i);
    i = i + 1;
}

// for 循环
for (var i = 0; i < 10; i = i + 1) {
    print(i);
}

// foreach 循环
var numbers = [1, 2, 3, 4, 5];
foreach (var num in numbers) {
    print(num);
}
```

#### switch 语句

```KrtL
switch (day) {
    case 0:
        print("Sunday");
        break;
    case 1:
        print("Monday");
        break;
    default:
        print("Other day");
        break;
}
```

### 函数

#### 函数定义和调用

```KrtL
// 函数定义
function Add(a: int32, b: int32): int32 {
    return a + b;
}

// 函数调用
var result = Add(5, 3);
print(result);  // 输出 8
```

#### 参数传递

```KrtL
// 值传递
function Increment(x: int32): int32 {
    x = x + 1;
    return x;
}

var a = 5;
var b = Increment(a);  // a 仍然是 5

// 引用传递
function Swap(ref a: int32, ref b: int32) {
    var temp = a;
    a = b;
    b = temp;
}

var x = 5, y = 10;
Swap(ref x, ref y);     // x = 10, y = 5
```

#### 递归函数

```KrtL
function Factorial(n: int32): int32 {
    if (n <= 1) {
        return 1;
    }
    return n * Factorial(n - 1);
}

var result = Factorial(5);
print(result);  // 输出 120
```

### 数组

#### 数组声明和初始化

```KrtL
// 数组声明
int32[] numbers;
string[] names;

// 数组初始化
int32[] numbers = [1, 2, 3, 4, 5];
string[] names = ["Alice", "Bob", "Charlie"];

// 动态数组
int32[] dynamicArray = new int32[10];
```

#### 数组操作

```KrtL
var numbers = [1, 2, 3, 4, 5];

// 访问元素
var first = numbers[0];
var last = numbers[4];

// 修改元素
numbers[0] = 10;

// 获取数组长度
var length = numbers.length;
print(length);  // 输出 5

// 遍历数组
for (var i = 0; i < numbers.length; i = i + 1) {
    print(numbers[i]);
}

// 使用 foreach 遍历
foreach (var num in numbers) {
    print(num);
}
```

## 高级特性

### 面向对象编程

#### 类定义

```KrtL
class Person {
    // 私有字段
    private var name;
    private var age: int32;
    
    // 构造函数
    function Person(name, age: int32) {
        this.name = name;
        this.age = age;
    }
    
    // 公共方法
    public function GetName() {
        return this.name;
    }
    
    public function GetAge(): int32 {
        return this.age;
    }
    
    public function SetAge(age: int32) {
        if (age >= 0) {
            this.age = age;
        }
    }
    
    // 静态方法
    public static function CreateDefault(): Person {
        return new Person("Unknown", 0);
    }
}
```

#### 对象创建和使用

```KrtL
// 创建对象
var person = new Person("Alice", 30);

// 访问方法
var name = person.GetName();
var age = person.GetAge();
print(name);  // 输出 "Alice"
print(age);   // 输出 30

// 修改属性
person.SetAge(31);
age = person.GetAge();
print(age);   // 输出 31

// 使用静态方法
var defaultPerson = Person.CreateDefault();
```

#### 继承

```KrtL
class Animal {
    protected var name;
    
    function Animal(name) {
        this.name = name;
    }
    
    public function Speak() {
        print("Animal sound");
    }
}

class Dog : Animal {
    private var breed;
    
    function Dog(name, breed): base(name) {
        this.breed = breed;
    }
    
    public override function Speak() {
        print("Woof!");
    }
    
    public function GetBreed() {
        return this.breed;
    }
}

// 使用继承
var dog = new Dog("Buddy", "Golden Retriever");
dog.Speak();  // 输出 "Woof!"
var breed = dog.GetBreed();
print(breed); // 输出 "Golden Retriever"
```

#### 接口

```KrtL
interface IDrawable {
    function Draw();
}

interface IMovable {
    function Move(x: int32, y: int32);
}

class Circle : IDrawable, IMovable {
    private var x: int32;
    private var y: int32;
    private var radius: int32;
    
    function Circle(x: int32, y: int32, radius: int32) {
        this.x = x;
        this.y = y;
        this.radius = radius;
    }
    
    public function Draw() {
        print("Drawing a circle at (" + this.x + ", " + this.y + ") with radius " + this.radius);
    }
    
    public function Move(x: int32, y: int32) {
        this.x = x;
        this.y = y;
    }
}

// 使用接口
var circle = new Circle(10, 20, 5);
circle.Draw();      // 输出 "Drawing a circle at (10, 20) with radius 5"
circle.Move(30, 40);
circle.Draw();      // 输出 "Drawing a circle at (30, 40) with radius 5"
```

### 泛型编程

#### 泛型类

```KrtL
class Box<T> {
    private var value: T;
    
    function Box(value: T) {
        this.value = value;
    }
    
    public function GetValue(): T {
        return this.value;
    }
    
    public function SetValue(value: T) {
        this.value = value;
    }
}

// 使用泛型类
var intBox = new Box<int32>(42);
var stringBox = new Box<string>("Hello");

var intValue = intBox.GetValue();
var stringValue = stringBox.GetValue();

print(intValue);    // 输出 42
print(stringValue); // 输出 "Hello"
```

#### 泛型函数

```KrtL
function Swap<T>(ref a: T, ref b: T) {
    var temp = a;
    a = b;
    b = temp;
}

// 使用泛型函数
var x = 5, y = 10;
Swap<int32>(ref x, ref y);
print(x);  // 输出 10
print(y);  // 输出 5

var s1 = "Hello", s2 = "World";
Swap<string>(ref s1, ref s2);
print(s1); // 输出 "World"
print(s2); // 输出 "Hello"
```

### 异常处理

#### 异常抛出和捕获

```KrtL
function Divide(a: float64, b: float64): float64 {
    if (b == 0.0) {
        throw new Exception("Division by zero");
    }
    return a / b;
}

try {
    var result = Divide(10.0, 0.0);
    print(result);
} catch (Exception e) {
    print("Error: " + e.Message);
} finally {
    print("Cleanup code");
}
```

#### 自定义异常

```KrtL
class InvalidArgumentException : Exception {
    function InvalidArgumentException(message): base(message) {
    }
}

function ProcessAge(age: int32) {
    if (age < 0 || age > 150) {
        throw new InvalidArgumentException("Invalid age: " + age);
    }
    print("Processing age: " + age);
}

try {
    ProcessAge(-5);
} catch (InvalidArgumentException e) {
    print("Invalid argument: " + e.Message);
}
```

### 命名空间

#### 命名空间声明

```KrtL
namespace MyCompany.MyApp {
    class MyClass {
        public function SayHello() {
            print("Hello from MyApp!");
        }
    }
    
    function UtilityFunction() {
        print("Utility function");
    }
}
```

#### 命名空间使用

```KrtL
using MyCompany.MyApp;

function main() {
    var obj = new MyClass();
    obj.SayHello();
    
    UtilityFunction();
}
```

#### 命名空间别名

```KrtL
using MyApp = MyCompany.MyApp;

function main() {
    var obj = new MyApp.MyClass();
    obj.SayHello();
}
```

## 标准库

### 输入输出

#### 控制台输出

```KrtL
// 基本输出
print("Hello, World!");

// 格式化输出
var name = "Alice";
var age = 30;
print("Name: " + name + ", Age: " + age);

// 使用Console类
Console.WriteLine("Hello, World!");
Console.Write("Enter your name: ");
```

#### 控制台输入

```KrtL
// 读取单个字符
var c = read_char();
print("You entered: " + c);

// 读取字符串（需要实现）
var input = read_line();
print("You entered: " + input);
```

### 文件操作

#### 文件读写

```KrtL
// 写入文件
var file = fopen("output.txt", "w");
if (file != null) {
    fwrite("Hello, File!", 1, 12, file);
    fclose(file);
}

// 读取文件
file = fopen("input.txt", "r");
if (file != null) {
    var buffer = malloc(100);
    var bytesRead = fread(buffer, 1, 99, file);
    buffer[bytesRead] = '\0';
    print(buffer);
    free(buffer);
    fclose(file);
}
```

### 数学函数

```KrtL
// 基本数学运算
var result = sqrt(16);      // 4.0
result = pow(2, 3);         // 8.0
result = abs(-5);           // 5

// 三角函数
result = sin(3.14159 / 2);  // 1.0
result = cos(0);            // 1.0
result = tan(3.14159 / 4);  // 1.0

// 对数函数
result = log(2.71828);      // 1.0
result = log10(100);        // 2.0
```

### 时间和日期

```KrtL
// 获取当前时间
var currentTime = time();
print("Current time: " + currentTime);

// 格式化时间
var formattedTime = time_format("%Y-%m-%d %H:%M:%S");
print("Formatted time: " + formattedTime);

// 计时器
var timer = timer_start();
// 执行一些操作
sleep(1);
var elapsed = timer_elapsed();
print("Elapsed time: " + elapsed + " seconds");
```

### 字符串操作

```KrtL
var str1 = "Hello";
var str2 = "World";

// 字符串连接
var result = str1 + ", " + str2;
print(result);  // 输出 "Hello, World"

// 字符串长度
var len = strlen(str1);
print(len);     // 输出 5

// 字符串比较
if (strcmp(str1, str2) == 0) {
    print("Strings are equal");
} else {
    print("Strings are different");
}

// 字符串查找
var pos = strstr(result, "World");
if (pos != null) {
    print("Found 'World' in the string");
}
```

## 项目构建

### 单文件项目

对于简单的单文件项目，可以直接编译：

```bash
e_sharp hello.es
./hello
```

### 多文件项目

对于多文件项目，可以使用项目文件：

#### 创建项目文件

创建 `myproject.esproj` 文件：

```json
{
    "name": "MyProject",
    "version": "1.0.0",
    "entry": "main.es",
    "sources": [
        "main.es",
        "utils.es",
        "models.es"
    ],
    "dependencies": [],
    "output": {
        "file": "myproject.exe",
        "target": "x86"
    },
    "compiler": {
        "optimization": "O2",
        "debug": false
    }
}
```

#### 编译项目

```bash
e_sharp -p myproject.esproj
./myproject
```

### 构建配置

#### 调试构建

```bash
e_sharp -g -O0 input.es
```

#### 发布构建

```bash
e_sharp -O3 input.es
```

#### 指定目标平台

```bash
e_sharp -target x86 input.es     # x86平台
e_sharp -target x64 input.es     # x64平台
e_sharp -target wasm input.es    # WebAssembly
```

## 常见问题

### 编译错误

#### "未定义的标识符"

**问题：** 使用了未声明的变量或函数

**解决方案：** 确保所有变量和函数在使用前已声明

```KrtL
// 错误
print(x);  // x未声明

// 正确
var x = 10;
print(x);
```

#### "类型不匹配"

**问题：** 尝试将不兼容的类型赋值

**解决方案：** 确保类型匹配或进行适当的类型转换

```KrtL
// 错误
var x: int32 = "hello";  // 类型不匹配

// 正确
var x: int32 = 42;
var y = "hello";
```

#### "函数参数数量不匹配"

**问题：** 调用函数时参数数量不正确

**解决方案：** 确保调用函数时提供正确数量的参数

```KrtL
// 错误
Add(5);  // Add函数需要两个参数

// 正确
Add(5, 3);
```

### 运行时错误

#### "除零错误"

**问题：** 尝试除以零

**解决方案：** 在除法操作前检查除数是否为零

```KrtL
// 错误
var result = 10 / 0;

// 正确
var b = 0;
if (b != 0) {
    var result = 10 / b;
} else {
    print("Cannot divide by zero");
}
```

#### "数组索引越界"

**问题：** 访问数组时索引超出范围

**解决方案：** 确保数组索引在有效范围内

```KrtL
var numbers = [1, 2, 3, 4, 5];

// 错误
var value = numbers[10];  // 索引超出范围

// 正确
if (i >= 0 && i < numbers.length) {
    var value = numbers[i];
}
```

#### "空指针引用"

**问题：** 尝试访问空对象的成员

**解决方案：** 在访问对象成员前检查对象是否为空

```KrtL
Person person = null;

// 错误
var name = person.GetName();  // 空指针引用

// 正确
if (person != null) {
    var name = person.GetName();
}
```

### 性能问题

#### 程序运行缓慢

**可能原因：**
1. 算法效率低
2. 不必要的内存分配
3. 缺少编译器优化

**解决方案：**
1. 使用更高效的算法
2. 减少内存分配和释放
3. 启用编译器优化（-O2或-O3）

#### 内存使用过多

**可能原因：**
1. 内存泄漏
2. 不必要的数据结构
3. 大量临时对象

**解决方案：**
1. 确保所有分配的内存都被释放
2. 使用适当的数据结构
3. 减少临时对象的创建

## 最佳实践

### 代码风格

#### 命名约定

- 类名：大驼峰命名（如 `MyClass`）
- 函数名：大驼峰命名（如 `MyFunction`）
- 变量名：小驼峰命名（如 `myVariable`）
- 常量名：全大写，下划线分隔（如 `MAX_SIZE`）

#### 代码组织

```KrtL
namespace MyNamespace {
    class MyClass {
        // 字段
        private var myField: int32;
        
        // 构造函数
        function MyClass() {
            // 初始化代码
        }
        
        // 公共方法
        public function MyMethod() {
            // 方法实现
        }
        
        // 私有方法
        private function HelperMethod() {
            // 辅助方法实现
        }
    }
}
```

#### 注释

```KrtL
/**
 * 计算两个数的和
 * @param a 第一个数
 * @param b 第二个数
 * @return 两数之和
 */
function Add(a: int32, b: int32): int32 {
    return a + b;
}
```

### 性能优化

#### 避免不必要的内存分配

```KrtL
// 不推荐：在循环中创建临时对象
for (var i = 0; i < 1000; i = i + 1) {
    var temp = new StringBuilder();
    temp.Append("Item ");
    temp.Append(i);
    print(temp.ToString());
}

// 推荐：重用对象
var temp = new StringBuilder();
for (var i = 0; i < 1000; i = i + 1) {
    temp.Clear();
    temp.Append("Item ");
    temp.Append(i);
    print(temp.ToString());
}
```

#### 使用适当的数据结构

```KrtL
// 不推荐：在列表中频繁查找元素
var list = [1, 2, 3, 4, 5];
for (var i = 0; i < 10000; i = i + 1) {
    var index = list.indexOf(3);  // O(n)操作
}

// 推荐：使用哈希表进行频繁查找
var map = new HashMap<int32, int32>();
map.put(1, 0);
map.put(2, 1);
map.put(3, 2);
map.put(4, 3);
map.put(5, 4);
for (var i = 0; i < 10000; i = i + 1) {
    var index = map.get(3);  // O(1)操作
}
```

### 错误处理

#### 使用异常处理错误情况

```KrtL
// 不推荐：使用错误码
function Divide(a: float64, b: float64): int32 {
    if (b == 0.0) {
        return -1;  // 错误码
    }
    return a / b;
}

var result = Divide(10.0, 0.0);
if (result == -1) {
    print("Division error");
}

// 推荐：使用异常
function Divide(a: float64, b: float64): float64 {
    if (b == 0.0) {
        throw new Exception("Division by zero");
    }
    return a / b;
}

try {
    var result = Divide(10.0, 0.0);
    print(result);
} catch (Exception e) {
    print("Error: " + e.Message);
}
```

#### 使用RAII管理资源

```KrtL
// 不推荐：手动管理资源
var file = fopen("data.txt", "r");
if (file != null) {
    // 使用文件
    if (error_occurred) {
        fclose(file);
        return;
    }
    // 更多操作
    fclose(file);
}

// 推荐：使用using语句自动管理资源
using (var file = fopen("data.txt", "r")) {
    // 使用文件
    if (error_occurred) {
        return;  // 文件会自动关闭
    }
    // 更多操作
    // 文件会在作用域结束时自动关闭
}
```

### 测试

#### 单元测试

```KrtL
class TestAdd {
    public function TestPositiveNumbers() {
        var result = Add(5, 3);
        assert(result == 8);
    }
    
    public function TestNegativeNumbers() {
        var result = Add(-5, -3);
        assert(result == -8);
    }
    
    public function TestMixedNumbers() {
        var result = Add(5, -3);
        assert(result == 2);
    }
}

function main() {
    var test = new TestAdd();
    test.TestPositiveNumbers();
    test.TestNegativeNumbers();
    test.TestMixedNumbers();
    print("All tests passed!");
}
```

#### 集成测试

```KrtL
function TestFileOperations() {
    // 写入测试文件
    var file = fopen("test.txt", "w");
    assert(file != null);
    fwrite("Test content", 1, 12, file);
    fclose(file);
    
    // 读取测试文件
    file = fopen("test.txt", "r");
    assert(file != null);
    var buffer = malloc(100);
    var bytesRead = fread(buffer, 1, 99, file);
    buffer[bytesRead] = '\0';
    assert(strcmp(buffer, "Test content") == 0);
    free(buffer);
    fclose(file);
    
    // 清理测试文件
    remove("test.txt");
}
```

---

*本用户手册涵盖了Kairote Lang语言的基础知识和使用方法。更多详细信息请参考语言规范和API参考文档。*