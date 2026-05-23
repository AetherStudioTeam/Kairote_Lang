# Kairote Lang 语言规范

## 目录

1. [概述](#概述)
2. [词法结构](#词法结构)
3. [语法结构](#语法结构)
4. [类型系统](#类型系统)
5. [表达式](#表达式)
6. [语句](#语句)
7. [函数](#函数)
8. [类和对象](#类和对象)
9. [命名空间](#命名空间)
10. [泛型编程](#泛型编程)
11. [异常处理](#异常处理)
12. [并发编程](#并发编程)

## 概述

Kairote Lang 是一种现代的、面向对象的编程语言，设计目标是提供高性能、类型安全和易用性的平衡。它融合了多种编程语言的特性，包括 C++ 的性能、C# 的语法和 Java 的跨平台能力。

### 主要特性

- 强类型系统
- 面向对象编程（类、继承、多态）
- 泛型编程
- 垃圾回收与手动内存管理
- 异常处理机制
- 命名空间支持
- 函数式编程特性
- 跨平台编译

## 词法结构

### 关键字

Kairote Lang 保留以下关键字：

```
function, var, if, else, while, for, foreach, in, return, print,
true, false, and, or, not, new, delete, class, struct, interface,
enum, namespace, this, base, public, private, protected, static,
virtual, abstract, override, using, import, package, console,
try, catch, finally, throw, exception, template, typename, where,
switch, case, break, continue, default
```

### 数据类型关键字

```
int8, int16, int32, int64, uint8, uint16, uint32, uint64,
float32, float64, bool, char, void, string
```

### 标识符

标识符由字母、数字和下划线组成，必须以字母或下划线开头。标识符区分大小写。

### 注释

```KrtL
// 单行注释

/*
 * 多行注释
 */
```

### 字面量

#### 数字字面量

```KrtL
42          // 整数
3.14        // 浮点数
0xFF        // 十六进制
0b1010      // 二进制
```

#### 字符串字面量

```KrtL
"Hello, World!"    // 普通字符串
"Line 1\nLine 2"   // 包含转义字符的字符串
```

#### 布尔字面量

```KrtL
true
false
```

## 语法结构

### 程序结构

Kairote Lang 程序由一个或多个命名空间组成，每个命名空间可以包含类、结构、接口、枚举和函数。

```KrtL
namespace MyNamespace {
    class MyClass {
        // 类成员
    }
    
    function MyFunction() {
        // 函数体
    }
}
```

### 声明

#### 变量声明

```KrtL
var x = 10;              // 类型推断
int32 y = 20;           // 显式类型
string name = "Kairote Lang";     // 字符串类型
bool isReady = true;    // 布尔类型
```

#### 常量声明

```KrtL
const int32 MAX_SIZE = 100;
```

#### 数组声明

```KrtL
int32[] numbers = new int32[10];
string[] names = ["Alice", "Bob", "Charlie"];
```

## 类型系统

### 基本类型

| 类型 | 描述 | 大小 |
|------|------|------|
| int8 | 8位有符号整数 | 1字节 |
| int16 | 16位有符号整数 | 2字节 |
| int32 | 32位有符号整数 | 4字节 |
| int64 | 64位有符号整数 | 8字节 |
| uint8 | 8位无符号整数 | 1字节 |
| uint16 | 16位无符号整数 | 2字节 |
| uint32 | 32位无符号整数 | 4字节 |
| uint64 | 64位无符号整数 | 8字节 |
| float32 | 32位浮点数 | 4字节 |
| float64 | 64位浮点数 | 8字节 |
| bool | 布尔值 | 1字节 |
| char | 字符 | 2字节 |
| string | 字符串 | 可变 |
| void | 无类型 | - |

### 复合类型

#### 数组

```KrtL
int32[] numbers;        // 整数数组
string[] names;         // 字符串数组
int32[][] matrix;       // 二维数组
```

#### 类

```KrtL
class Person {
    var name;
    var age: int32;
    
    function Person(name, age: int32) {
        this.name = name;
        this.age = age;
    }
}
```

#### 结构

```KrtL
struct Point {
    var x: float32;
    var y: float32;
}
```

### 类型转换

#### 隐式转换

```KrtL
int32 i = 42;
float64 f = i;          // int32 到 float64 的隐式转换
```

#### 显式转换

```KrtL
float64 f = 3.14;
int32 i = (int32)f;     // 显式转换
```

## 表达式

### 算术表达式

```KrtL
var a = 10 + 5;         // 加法
var b = 10 - 5;         // 减法
var c = 10 * 5;         // 乘法
var d = 10 / 5;         // 除法
var e = 10 % 5;         // 取模
var f = 2 ** 3;         // 幂运算
```

### 比较表达式

```KrtL
var a = 10 == 5;        // 等于
var b = 10 != 5;        // 不等于
var c = 10 > 5;         // 大于
var d = 10 < 5;         // 小于
var e = 10 >= 5;        // 大于等于
var f = 10 <= 5;        // 小于等于
```

### 逻辑表达式

```KrtL
var a = true and false; // 逻辑与
var b = true or false;  // 逻辑或
var c = not true;       // 逻辑非
```

### 位运算表达式

```KrtL
var a = 5 & 3;          // 按位与
var b = 5 | 3;          // 按位或
var c = 5 ^ 3;          // 按位异或
var d = ~5;             // 按位取反
var e = 5 << 2;         // 左移
var f = 5 >> 2;         // 右移
```

### 三元表达式

```KrtL
var result = (x > 0) ? "positive" : "non-positive";
```

## 语句

### 条件语句

#### if 语句

```KrtL
if (x > 0) {
    print("x is positive");
} else if (x < 0) {
    print("x is negative");
} else {
    print("x is zero");
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

### 循环语句

#### while 循环

```KrtL
var i = 0;
while (i < 10) {
    print(i);
    i = i + 1;
}
```

#### for 循环

```KrtL
for (var i = 0; i < 10; i = i + 1) {
    print(i);
}
```

#### foreach 循环

```KrtL
var numbers = [1, 2, 3, 4, 5];
foreach (var num in numbers) {
    print(num);
}
```

### 跳转语句

```KrtL
for (var i = 0; i < 10; i = i + 1) {
    if (i == 5) {
        break;          // 跳出循环
    }
    if (i % 2 == 0) {
        continue;       // 跳过本次迭代
    }
    print(i);
}
```

## 函数

### 函数声明

```KrtL
function Add(a: int32, b: int32): int32 {
    return a + b;
}
```

### 函数调用

```KrtL
var result = Add(5, 3);
```

### 参数传递

#### 值传递

```KrtL
function Increment(x: int32): int32 {
    x = x + 1;
    return x;
}

var a = 5;
var b = Increment(a);  // a 仍然是 5
```

#### 引用传递

```KrtL
function Swap(ref a: int32, ref b: int32) {
    var temp = a;
    a = b;
    b = temp;
}

var x = 5, y = 10;
Swap(ref x, ref y);     // x = 10, y = 5
```

### 递归函数

```KrtL
function Factorial(n: int32): int32 {
    if (n <= 1) {
        return 1;
    }
    return n * Factorial(n - 1);
}
```

### 匿名函数

```KrtL
var add = function(a: int32, b: int32): int32 {
    return a + b;
};

var result = add(5, 3);
```

## 类和对象

### 类定义

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
}
```

### 对象创建

```KrtL
var person = new Person("Alice", 30);
print(person.GetName());
print(person.GetAge());
```

### 继承

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
}
```

### 接口

```KrtL
interface IDrawable {
    function Draw();
}

class Circle : IDrawable {
    private var radius: float32;
    
    function Circle(radius: float32) {
        this.radius = radius;
    }
    
    public function Draw() {
        print("Drawing a circle with radius " + this.radius);
    }
}
```

### 抽象类

```KrtL
abstract class Shape {
    protected var name;
    
    function Shape(name) {
        this.name = name;
    }
    
    public abstract function GetArea(): float32;
    
    public function GetName() {
        return this.name;
    }
}

class Rectangle : Shape {
    private var width: float32;
    private var height: float32;
    
    function Rectangle(width: float32, height: float32): base("Rectangle") {
        this.width = width;
        this.height = height;
    }
    
    public override function GetArea(): float32 {
        return this.width * this.height;
    }
}
```

## 命名空间

### 命名空间声明

```KrtL
namespace MyCompany.MyApp {
    class MyClass {
        // 类实现
    }
}
```

### 命名空间导入

```KrtL
using MyCompany.MyApp;

function Main() {
    var obj = new MyClass();
}
```

### 命名空间别名

```KrtL
using MyApp = MyCompany.MyApp;

function Main() {
    var obj = new MyApp.MyClass();
}
```

## 泛型编程

### 泛型类

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

var intBox = new Box<int32>(42);
var stringBox = new Box<string>("Hello");
```

### 泛型函数

```KrtL
function Swap<T>(ref a: T, ref b: T) {
    var temp = a;
    a = b;
    b = temp;
}

var x = 5, y = 10;
Swap<int32>(ref x, ref y);
```

### 泛型约束

```KrtL
interface IComparable<T> {
    function CompareTo(other: T): int32;
}

function Max<T where T: IComparable<T>>(a: T, b: T): T {
    if (a.CompareTo(b) > 0) {
        return a;
    }
    return b;
}
```

## 异常处理

### 异常抛出

```KrtL
function Divide(a: float64, b: float64): float64 {
    if (b == 0.0) {
        throw new Exception("Division by zero");
    }
    return a / b;
}
```

### 异常捕获

```KrtL
try {
    var result = Divide(10.0, 0.0);
    print(result);
} catch (Exception e) {
    print("Error: " + e.Message);
} finally {
    print("Cleanup code");
}
```

### 自定义异常

```KrtL
class InvalidArgumentException : Exception {
    function InvalidArgumentException(message): base(message) {
    }
}

function ProcessAge(age: int32) {
    if (age < 0 || age > 150) {
        throw new InvalidArgumentException("Invalid age: " + age);
    }
    // 处理年龄
}
```

## 并发编程

### 线程创建

```KrtL
using System.Threading;

function ThreadFunction() {
    for (var i = 0; i < 5; i = i + 1) {
        print("Thread: " + i);
        Thread.Sleep(1000);
    }
}

var thread = new Thread(ThreadFunction);
thread.Start();
```

### 同步机制

```KrtL
class Counter {
    private var count: int32 = 0;
    private var lock = new Object();
    
    public function Increment() {
        lock (this.lock) {
            this.count = this.count + 1;
        }
    }
    
    public function GetCount(): int32 {
        lock (this.lock) {
            return this.count;
        }
    }
}
```

## 内存管理

### 垃圾回收

Kairote Lang 使用自动垃圾回收机制管理内存，但开发者也可以手动控制内存：

```KrtL
// 自动内存管理
var obj = new MyClass();
// obj 在不再被引用时会被自动回收

// 手动内存管理
var ptr = malloc(1024);
// 使用内存
free(ptr);
```

### 资源管理

```KrtL
function ProcessFile(filename) {
    var file = new File(filename);
    try {
        // 使用文件
    } finally {
        file.Close();
    }
}

// 使用 using 语句自动管理资源
function ProcessFileWithUsing(filename) {
    using (var file = new File(filename)) {
        // 使用文件
        // 文件会在作用域结束时自动关闭
    }
}
```

---

*本规范文档涵盖了 Kairote Lang 语言的核心特性，更多详细信息请参考 API 参考文档和开发者指南。