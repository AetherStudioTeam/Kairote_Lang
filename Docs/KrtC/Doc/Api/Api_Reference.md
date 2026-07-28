# KrtLang API 参考文档

## 目录

1. [输入输出](#输入输出)
2. [内存管理](#内存管理)
3. [字符串操作](#字符串操作)
4. [数学函数](#数学函数)
5. [时间与日期](#时间与日期)
6. [文件操作](#文件操作)
7. [数组操作](#数组操作)
8. [哈希表操作](#哈希表操作)
9. [网络编程](#网络编程)
10. [多线程](#多线程)
11. [图形界面](#图形界面)
12. [错误处理](#错误处理)

## 输入输出
### print

打印字符串到控制台。

```KrtL
function print(str)
```

**参数:**
- `str`: 要打印的字符串

**示例:**
```KrtL
print("Hello, World!");
```

### Console__WriteLine

向控制台写入一行文本。

```KrtL
function Console__WriteLine(str)
```

**参数:**
- `str`: 要写入的字符串

**示例:**
```KrtL
Console__WriteLine("Hello, KrtLang!");
```

### Console__Write

向控制台写入文本，不换行。

```KrtL
function Console__Write(str)
```

**参数:**
- `str`: 要写入的字符串

**示例:**
```KrtL
Console__Write("Hello, ");
Console__Write("KrtLang!");
```

### Console__ReadLine

从控制台读取一行文本。

```KrtL
function Console__ReadLine()
```

**返回值:**
- 读取的字符串，包含换行符

**示例:**
```KrtL
var line = Console__ReadLine();
```

## 内存管理

### KrtMalloc

分配指定大小的内存块。

```KrtL
function KrtMalloc(size: size_t)*
```

**参数:**
- `size`: 要分配的内存大小（字节）

**返回值:**
- 指向分配的内存块的指针，如果分配失败则返回null

**示例:**
```KrtL
var ptr = KrtMalloc(1024);
if (ptr != null) {
    // 使用内存
    KrtFree(ptr);
}
```

### KrtFree

释放之前分配的内存块。

```KrtL
function KrtFree(ptr*)
```

**参数:**
- `ptr`: 要释放的内存块指针

**示例:**
```KrtL
var ptr = KrtMalloc(1024);
// 使用内存
KrtFree(ptr);
```

### KrtRealloc

重新分配内存块大小。

```KrtL
function KrtRealloc(ptr*, size: size_t)*
```

**参数:**
- `ptr`: 原始内存块指针
- `size`: 新的内存大小（字节）

**返回值:**
- 指向重新分配的内存块的指针，如果分配失败则返回null

**示例:**
```KrtL
var ptr = KrtMalloc(512);
ptr = KrtRealloc(ptr, 1024);
if (ptr != null) {
    // 使用更大的内存
    KrtFree(ptr);
}
```

### KrtCalloc

分配并初始化为零的内存块。

```KrtL
function KrtCalloc(num: size_t, size: size_t)*
```

**参数:**
- `num`: 元素数量
- `size`: 每个元素的大小（字节）

**返回值:**
- 指向分配并初始化为零的内存块的指针，如果分配失败则返回null

**示例:**
```KrtL
var ptr = KrtCalloc(10, 4);  // 分配10个4字节的元素，全部初始化为0
if (ptr != null) {
    // 使用内存
    KrtFree(ptr);
}
```

### get_total_memory

获取系统总内存大小。

```KrtL
function get_total_memory(): uint64
```

**返回值:**
- 系统总内存大小（字节）

**示例:**
```KrtL
var total = get_total_memory();
print("Total memory: ");
print(total / 1024 / 1024);
print(" MB");
```

### get_free_memory

获取系统可用内存大小。

```KrtL
function get_free_memory(): uint64
```

**返回值:**
- 系统可用内存大小（字节）

**示例:**
```KrtL
var free = get_free_memory();
print("Free memory: ");
print(free / 1024 / 1024);
print(" MB");
```

## 字符串操作

### KrtStrlen

计算字符串长度。

```KrtL
function KrtStrlen(str): size_t
```

**参数:**
- `str`: 要计算长度的字符串

**返回值:**
- 字符串的长度（不包括终止空字符）

**示例:**
```KrtL
var len = KrtStrlen("Hello");
print(len);  // 输出 5
```

### KrtStrcpy

复制字符串。

```KrtL
function KrtStrcpy(dest, src)
```

**参数:**
- `dest`: 目标字符串缓冲区
- `src`: 源字符串

**返回值:**
- 目标字符串的指针

**示例:**
```KrtL
var dest = KrtMalloc(100);
KrtStrcpy(dest, "Hello, World!");
print(dest);
KrtFree(dest);
```

### KrtStrcat

连接字符串。

```KrtL
function KrtStrcat(dest, src)
```

**参数:**
- `dest`: 目标字符串（必须有足够的空间）
- `src`: 要追加的源字符串

**返回值:**
- 目标字符串的指针

**示例:**
```KrtL
var dest = KrtMalloc(100);
KrtStrcpy(dest, "Hello, ");
KrtStrcat(dest, "World!");
print(dest);
KrtFree(dest);
```

### KrtStrcmp

比较两个字符串。

```KrtL
function KrtStrcmp(str1, str2): int
```

**参数:**
- `str1`: 第一个字符串
- `str2`: 第二个字符串

**返回值:**
- 如果str1 < str2，返回负数
- 如果str1 == str2，返回0
- 如果str1 > str2，返回正数

**示例:**
```KrtL
var result = KrtStrcmp("apple", "banana");
if (result < 0) {
    print("apple comes before banana");
}
```

### KrtStrdup

复制字符串（动态分配内存）。

```KrtL
function KrtStrdup(src)
```

**参数:**
- `src`: 要复制的源字符串

**返回值:**
- 新分配的字符串副本，使用后需要用KrtFree释放

**示例:**
```KrtL
var copy = KrtStrdup("Hello, World!");
print(copy);
KrtFree(copy);
```

### KrtStrchr

在字符串中查找字符。

```KrtL
function KrtStrchr(str, c: int)
```

**参数:**
- `str`: 要搜索的字符串
- `c`: 要查找的字符

**返回值:**
- 指向字符串中第一次出现字符c的位置的指针，如果未找到则返回null

**示例:**
```KrtL
var pos = KrtStrchr("Hello, World!", 'W');
if (pos != null) {
    print(pos);  // 输出 "World!"
}
```

### KrtStrstr

在字符串中查找子字符串。

```KrtL
function KrtStrstr(haystack, needle)
```

**参数:**
- `haystack`: 要搜索的字符串
- `needle`: 要查找的子字符串

**返回值:**
- 指向字符串中第一次出现子字符串的位置的指针，如果未找到则返回null

**示例:**
```KrtL
var pos = KrtStrstr("Hello, World!", "World");
if (pos != null) {
    print(pos);  // 输出 "World!"
}
```

### KrtAtoi

将字符串转换为整数。

```KrtL
function KrtAtoi(str): int
```

**参数:**
- `str`: 要转换的字符串

**返回值:**
- 转换后的整数值

**示例:**
```KrtL
var num = KrtAtoi("123");
print(num);  // 输出 123
```

### KrtAtof

将字符串转换为浮点数。

```KrtL
function KrtAtof(str): double
```

**参数:**
- `str`: 要转换的字符串

**返回值:**
- 转换后的浮点数值

**示例:**
```KrtL
var num = KrtAtof("3.14159");
print(num);  // 输出 3.14159
```

## 数学函数

### KrtSin

计算正弦值。

```KrtL
function KrtSin(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 正弦值

**示例:**
```KrtL
var result = KrtSin(3.14159 / 2);
print(result);  // 输出约 1.0
```

### KrtCos

计算余弦值。

```KrtL
function KrtCos(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 余弦值

**示例:**
```KrtL
var result = KrtCos(0);
print(result);  // 输出 1.0
```

### KrtTan

计算正切值。

```KrtL
function KrtTan(x: double): double
```

**参数:**
- `x`: 角度（弧度）

**返回值:**
- 正切值

**示例:**
```KrtL
var result = KrtTan(3.14159 / 4);
print(result);  // 输出约 1.0
```

### KrtSqrt

计算平方根。

```KrtL
function KrtSqrt(x: double): double
```

**参数:**
- `x`: 要计算平方根的数值

**返回值:**
- 平方根值

**示例:**
```KrtL
var result = KrtSqrt(16);
print(result);  // 输出 4.0
```

### KrtPow

计算幂。

```KrtL
function KrtPow(base: double, exp: double): double
```

**参数:**
- `base`: 底数
- `exp`: 指数

**返回值:**
- base的exp次幂

**示例:**
```KrtL
var result = KrtPow(2, 3);
print(result);  // 输出 8.0
```

### KrtLog

计算自然对数。

```KrtL
function KrtLog(x: double): double
```

**参数:**
- `x`: 要计算对数的数值

**返回值:**
- 自然对数值

**示例:**
```KrtL
var result = KrtLog(2.71828);
print(result);  // 输出约 1.0
```

### KrtLog10

计算以10为底的对数。

```KrtL
function KrtLog10(x: double): double
```

**参数:**
- `x`: 要计算对数的数值

**返回值:**
- 以10为底的对数值

**示例:**
```KrtL
var result = KrtLog10(100);
print(result);  // 输出 2.0
```

### KrtExp

计算e的x次幂。

```KrtL
function KrtExp(x: double): double
```

**参数:**
- `x`: 指数

**返回值:**
- e的x次幂

**示例:**
```KrtL
var result = KrtExp(1);
print(result);  // 输出约 2.71828
```

### KrtFabs

计算绝对值。

```KrtL
function KrtFabs(x: double): double
```

**参数:**
- `x`: 要计算绝对值的数值

**返回值:**
- 绝对值

**示例:**
```KrtL
var result = KrtFabs(-3.14);
print(result);  // 输出 3.14
```

### KrtAbs

计算整数的绝对值。

```KrtL
function KrtAbs(x: int): int
```

**参数:**
- `x`: 要计算绝对值的整数

**返回值:**
- 绝对值

**示例:**
```KrtL
var result = KrtAbs(-42);
print(result);  // 输出 42
```

## 时间与日期

### KrtTime

获取当前时间（自1970年1月1日以来的秒数）。

```KrtL
function KrtTime(): double
```

**返回值:**
- 当前时间（秒）

**示例:**
```KrtL
var current = KrtTime();
print(current);
```

### KrtSleep

使当前线程休眠指定秒数。

```KrtL
function KrtSleep(seconds: int)
```

**参数:**
- `seconds`: 休眠的秒数

**示例:**
```KrtL
print("Going to sleep for 2 seconds...");
KrtSleep(2);
print("Awake!");
```

### KrtDate

获取当前日期字符串。

```KrtL
function KrtDate()
```

**返回值:**
- 当前日期字符串（需要释放内存）

**示例:**
```KrtL
var date = KrtDate();
print(date);
KrtFree(date);
```

### KrtTimeFormat

格式化时间字符串。

```KrtL
function KrtTimeFormat(format)
```

**参数:**
- `format`: 时间格式字符串

**返回值:**
- 格式化后的时间字符串（需要释放内存）

**示例:**
```KrtL
var time = KrtTimeFormat("%Y-%m-%d %H:%M:%S");
print(time);
KrtFree(time);
```

### timer_start

启动计时器。

```KrtL
function timer_start(): double
```

**返回值:**
- 计时器ID

**示例:**
```KrtL
var timer = timer_start();
// 执行一些操作
var elapsed = timer_elapsed();
print("Elapsed time: ");
print(elapsed);
print(" seconds");
```

### timer_elapsed

获取自计时器启动以来经过的时间。

```KrtL
function timer_elapsed(): double
```

**返回值:**
- 经过的时间（秒）

**示例:**
```KrtL
var timer = timer_start();
KrtSleep(1);
var elapsed = timer_elapsed();
print(elapsed);  // 输出约 1.0
```

## 文件操作

### KrtFopen

打开文件。

```KrtL
function KrtFopen(filename, mode): KRT_FILE
```

**参数:**
- `filename`: 文件名
- `mode`: 打开模式（"r"读取，"w"写入，"a"追加等）

**返回值:**
- 文件指针，如果打开失败则返回null

**示例:**
```KrtL
var file = KrtFopen("test.txt", "w");
if (file != null) {
    KrtFwrite("Hello, World!", 1, 13, file);
    KrtFclose(file);
}
```

### KrtFclose

关闭文件。

```KrtL
function KrtFclose(file: KRT_FILE): int
```

**参数:**
- `file`: 要关闭的文件指针

**返回值:**
- 如果成功关闭返回0，否则返回非零值

**示例:**
```KrtL
var file = KrtFopen("test.txt", "r");
if (file != null) {
    // 读取文件
    KrtFclose(file);
}
```

### KrtFread

从文件读取数据。

```KrtL
function KrtFread(buffer*, size: size_t, count: size_t, file: KRT_FILE): size_t
```

**参数:**
- `buffer`: 存储读取数据的缓冲区
- `size`: 每个元素的大小（字节）
- `count`: 要读取的元素数量
- `file`: 文件指针

**返回值:**
- 实际读取的元素数量

**示例:**
```KrtL
var buffer = KrtMalloc(100);
var file = KrtFopen("test.txt", "r");
if (file != null) {
    var bytesRead = KrtFread(buffer, 1, 99, file);
    buffer[bytesRead] = '\0';
    print(buffer);
    KrtFclose(file);
}
KrtFree(buffer);
```

### KrtFwrite

向文件写入数据。

```KrtL
function KrtFwrite(buffer*, size: size_t, count: size_t, file: KRT_FILE): size_t
```

**参数:**
- `buffer`: 包含要写入数据的缓冲区
- `size`: 每个元素的大小（字节）
- `count`: 要写入的元素数量
- `file`: 文件指针

**返回值:**
- 实际写入的元素数量

**示例:**
```KrtL
var text = "Hello, World!";
var file = KrtFopen("test.txt", "w");
if (file != null) {
    KrtFwrite(text, 1, KrtStrlen(text), file);
    KrtFclose(file);
}
```

### KrtFseek

设置文件位置指针。

```KrtL
function KrtFseek(file: KRT_FILE, offset: long, origin: int): int
```

**参数:**
- `file`: 文件指针
- `offset`: 偏移量
- `origin`: 起始位置（0=文件开始，1=当前位置，2=文件结尾）

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var file = KrtFopen("test.txt", "r");
if (file != null) {
    KrtFseek(file, 0, 2);  // 移动到文件末尾
    var size = KrtFtell(file);
    print("File size: ");
    print(size);
    KrtFclose(file);
}
```

### KrtFtell

获取当前文件位置。

```KrtL
function KrtFtell(file: KRT_FILE): long
```

**参数:**
- `file`: 文件指针

**返回值:**
- 当前文件位置（从文件开始的字节数）

**示例:**
```KrtL
var file = KrtFopen("test.txt", "r");
if (file != null) {
    KrtFseek(file, 0, 2);  // 移动到文件末尾
    var size = KrtFtell(file);
    print("File size: ");
    print(size);
    KrtFclose(file);
}
```

### KrtRemove

删除文件。

```KrtL
function KrtRemove(filename): int
```

**参数:**
- `filename`: 要删除的文件名

**返回值:**
- 如果成功删除返回0，否则返回非零值

**示例:**
```KrtL
var result = KrtRemove("temp.txt");
if (result == 0) {
    print("File deleted successfully");
} else {
    print("Failed to delete file");
}
```

## 数组操作

### array_create

创建数组。

```KrtL
function array_create(element_size: size_t, initial_capacity: size_t): KrtArray*
```

**参数:**
- `element_size`: 每个元素的大小（字节）
- `initial_capacity`: 初始容量

**返回值:**
- 新创建的数组指针，如果创建失败则返回null

**示例:**
```KrtL
var arr = array_create(4, 10);  // 创建整型数组，初始容量为10
if (arr != null) {
    // 使用数组
    array_KrtFree(arr);
}
```

### array_KrtFree

释放数组内存。

```KrtL
function array_KrtFree(array: KrtArray*)
```

**参数:**
- `array`: 要释放的数组指针

**示例:**
```KrtL
var arr = array_create(4, 10);
// 使用数组
array_KrtFree(arr);
```

### array_size

获取数组当前大小。

```KrtL
function array_size(array: KrtArray*): size_t
```

**参数:**
- `array`: 数组指针

**返回值:**
- 数组中元素的数量

**示例:**
```KrtL
var arr = array_create(4, 10);
var size = array_size(arr);
print(size);  // 输出 0
array_KrtFree(arr);
```

### array_append

向数组添加元素。

```KrtL
function array_append(array: KrtArray*, element*): int
```

**参数:**
- `array`: 数组指针
- `element`: 要添加的元素指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var arr = array_create(4, 10);
var value = 42;
array_append(arr, &value);
var size = array_size(arr);
print(size);  // 输出 1
array_KrtFree(arr);
```

### array_get

获取数组中指定索引的元素。

```KrtL
function array_get(array: KrtArray*, index: size_t)*
```

**参数:**
- `array`: 数组指针
- `index`: 元素索引

**返回值:**
- 指向元素的指针，如果索引无效则返回null

**示例:**
```KrtL
var arr = array_create(4, 10);
var value = 42;
array_append(arr, &value);
var ptr = array_get(arr, 0);
if (ptr != null) {
    var num = *(int*)ptr;
    print(num);  // 输出 42
}
array_KrtFree(arr);
```

### array_set

设置数组中指定索引的元素。

```KrtL
function array_set(array: KrtArray*, index: size_t, element*): int
```

**参数:**
- `array`: 数组指针
- `index`: 元素索引
- `element`: 要设置的元素指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var arr = array_create(4, 10);
var value1 = 42;
var value2 = 100;
array_append(arr, &value1);
array_set(arr, 0, &value2);
var ptr = array_get(arr, 0);
if (ptr != null) {
    var num = *(int*)ptr;
    print(num);  // 输出 100
}
array_KrtFree(arr);
```

## 哈希表操作

### hashmap_create

创建哈希表。

```KrtL
function hashmap_create(initial_capacity: size_t): KrtHashMap*
```

**参数:**
- `initial_capacity`: 初始容量

**返回值:**
- 新创建的哈希表指针，如果创建失败则返回null

**示例:**
```KrtL
var map = hashmap_create(10);
if (map != null) {
    // 使用哈希表
    hashmap_KrtFree(map);
}
```

### hashmap_KrtFree

释放哈希表内存。

```KrtL
function hashmap_KrtFree(map: KrtHashMap*)
```

**参数:**
- `map`: 要释放的哈希表指针

**示例:**
```KrtL
var map = hashmap_create(10);
// 使用哈希表
hashmap_KrtFree(map);
```

### hashmap_put

向哈希表添加键值对。

```KrtL
function hashmap_put(map: KrtHashMap*, key*, key_type: KrtHashMapType, value*, value_type: KrtHashMapType): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型
- `value`: 值指针
- `value_type`: 值类型

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var map = hashmap_create(10);
var key = "name";
var value = "KrtLang";
hashmap_put(map, &key, KRT_HASHMAP_TYPE_STRING, &value, KRT_HASHMAP_TYPE_STRING);
hashmap_KrtFree(map);
```

### hashmap_get

从哈希表获取值。

```KrtL
function hashmap_get(map: KrtHashMap*, key*, key_type: KrtHashMapType, value**, out_value_type: KrtHashMapType*): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型
- `value`: 存储值的指针的指针
- `out_value_type`: 存储值类型的指针

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var map = hashmap_create(10);
var key = "name";
var value = "KrtLang";
hashmap_put(map, &key, KRT_HASHMAP_TYPE_STRING, &value, KRT_HASHMAP_TYPE_STRING);

var retrieved_value;
var value_type;
if (hashmap_get(map, &key, KRT_HASHMAP_TYPE_STRING, &retrieved_value, &value_type) == 0) {
    var str = *(string*)retrieved_value;
    print(str);  // 输出 "KrtLang"
}
hashmap_KrtFree(map);
```

### hashmap_remove

从哈希表移除键值对。

```KrtL
function hashmap_remove(map: KrtHashMap*, key*, key_type: KrtHashMapType): int
```

**参数:**
- `map`: 哈希表指针
- `key`: 键指针
- `key_type`: 键类型

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var map = hashmap_create(10);
var key = "name";
var value = "KrtLang";
hashmap_put(map, &key, KRT_HASHMAP_TYPE_STRING, &value, KRT_HASHMAP_TYPE_STRING);
hashmap_remove(map, &key, KRT_HASHMAP_TYPE_STRING);
hashmap_KrtFree(map);
```

## 网络编程

### KrtSocket

创建套接字。

```KrtL
function KrtSocket(domain: int, type: int, protocol: int): KrtSocket
```

**参数:**
- `domain`: 地址族（如AF_INET）
- `type`: 套接字类型（如SOCK_STREAM）
- `protocol`: 协议类型（通常为0）

**返回值:**
- 套接字描述符，如果创建失败则返回无效套接字

**示例:**
```KrtL
var sock = KrtSocket(2, 1, 0);  // AF_INET, SOCK_STREAM
if (sock != null) {
    // 使用套接字
    KrtCloseSocket(sock);
}
```

### KrtConnect

连接到远程服务器。

```KrtL
function KrtConnect(socket: KrtSocket, address, port: int): int
```

**参数:**
- `socket`: 套接字描述符
- `address`: 服务器地址
- `port`: 服务器端口

**返回值:**
- 如果成功连接返回0，否则返回非零值

**示例:**
```KrtL
var sock = KrtSocket(2, 1, 0);
if (KrtConnect(sock, "example.com", 80) == 0) {
    print("Connected to server");
    KrtCloseSocket(sock);
} else {
    print("Connection failed");
}
```

### KrtSend

发送数据。

```KrtL
function KrtSend(socket: KrtSocket, data, length: size_t): int
```

**参数:**
- `socket`: 套接字描述符
- `data`: 要发送的数据
- `length`: 数据长度

**返回值:**
- 实际发送的字节数，如果出错返回-1

**示例:**
```KrtL
var sock = KrtSocket(2, 1, 0);
if (KrtConnect(sock, "example.com", 80) == 0) {
    var request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    KrtSend(sock, request, KrtStrlen(request));
    KrtCloseSocket(sock);
}
```

### KrtRecv

接收数据。

```KrtL
function KrtRecv(socket: KrtSocket, buffer, length: size_t): int
```

**参数:**
- `socket`: 套接字描述符
- `buffer`: 存储接收数据的缓冲区
- `length`: 缓冲区大小

**返回值:**
- 实际接收的字节数，如果出错返回-1，如果连接关闭返回0

**示例:**
```KrtL
var sock = KrtSocket(2, 1, 0);
if (KrtConnect(sock, "example.com", 80) == 0) {
    var request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    KrtSend(sock, request, KrtStrlen(request));
    
    var buffer = KrtMalloc(1024);
    var bytesRead = KrtRecv(sock, buffer, 1023);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        print(buffer);
    }
    KrtFree(buffer);
    KrtCloseSocket(sock);
}
```

## 多线程

### KrtThreadCreateFunc

创建线程。

```KrtL
function KrtThreadCreateFunc(func: function(void*), arg*): KrtThread
```

**参数:**
- `func`: 线程函数指针
- `arg`: 传递给线程函数的参数

**返回值:**
- 线程句柄，如果创建失败则返回null

**示例:**
```KrtL
function ThreadFunction(arg*) {
    print("Thread is running");
}

var thread = KrtThreadCreateFunc(ThreadFunction, null);
if (thread != null) {
    KrtThreadJoinFunc(thread);
}
```

### KrtThreadJoinFunc

等待线程结束。

```KrtL
function KrtThreadJoinFunc(thread: KrtThread): int
```

**参数:**
- `thread`: 线程句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
function ThreadFunction(arg*) {
    print("Thread is running");
}

var thread = KrtThreadCreateFunc(ThreadFunction, null);
if (thread != null) {
    KrtThreadJoinFunc(thread);
    print("Thread has finished");
}
```

### KrtMutexCreateFunc

创建互斥锁。

```KrtL
function KrtMutexCreateFunc(): KrtMutex
```

**返回值:**
- 互斥锁句柄，如果创建失败则返回null

**示例:**
```KrtL
var mutex = KrtMutexCreateFunc();
if (mutex != null) {
    KrtMutexLockFunc(mutex);
    // 临界区代码
    KrtMutexUnlockFunc(mutex);
    KrtMutexFreeFunc(mutex);
}
```

### KrtMutexLockFunc

锁定互斥锁。

```KrtL
function KrtMutexLockFunc(mutex: KrtMutex): int
```

**参数:**
- `mutex`: 互斥锁句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var mutex = KrtMutexCreateFunc();
if (mutex != null) {
    KrtMutexLockFunc(mutex);
    // 临界区代码
    KrtMutexUnlockFunc(mutex);
    KrtMutexFreeFunc(mutex);
}
```

### KrtMutexUnlockFunc

解锁互斥锁。

```KrtL
function KrtMutexUnlockFunc(mutex: KrtMutex): int
```

**参数:**
- `mutex`: 互斥锁句柄

**返回值:**
- 如果成功返回0，否则返回非零值

**示例:**
```KrtL
var mutex = KrtMutexCreateFunc();
if (mutex != null) {
    KrtMutexLockFunc(mutex);
    // 临界区代码
    KrtMutexUnlockFunc(mutex);
    KrtMutexFreeFunc(mutex);
}
```

## 图形界面

### KrtWindowCreate

创建窗口。

```KrtL
function KrtWindowCreate(width: int, height: int, title): KrtWindow
```

**参数:**
- `width`: 窗口宽度
- `height`: 窗口高度
- `title`: 窗口标题

**返回值:**
- 窗口句柄，如果创建失败则返回null

**示例:**
```KrtL
var window = KrtWindowCreate(800, 600, "My Window");
if (window != null) {
    KrtWindowShow(window);
    while (KrtWindowIsOpen(window)) {
        // 处理窗口事件
        KrtWindowUpdate(window);
    }
}
```

### KrtWindowShow

显示窗口。

```KrtL
function KrtWindowShow(window: KrtWindow)
```

**参数:**
- `window`: 窗口句柄

**示例:**
```KrtL
var window = KrtWindowCreate(800, 600, "My Window");
if (window != null) {
    KrtWindowShow(window);
    // 窗口事件循环
}
```

### KrtWindowClear

清除窗口内容。

```KrtL
function KrtWindowClear(window: KrtWindow, color: KrtColor)
```

**参数:**
- `window`: 窗口句柄
- `color`: 清除颜色

**示例:**
```KrtL
var window = KrtWindowCreate(800, 600, "My Window");
if (window != null) {
    KrtWindowShow(window);
    var black = {0, 0, 0, 255};
    KrtWindowClear(window, black);
    // 窗口事件循环
}
```

### KrtDrawRect

绘制矩形。

```KrtL
function KrtDrawRect(window: KrtWindow, rect: KrtRect, color: KrtColor)
```

**参数:**
- `window`: 窗口句柄
- `rect`: 矩形位置和大小
- `color`: 矩形颜色

**示例:**
```KrtL
var window = KrtWindowCreate(800, 600, "My Window");
if (window != null) {
    KrtWindowShow(window);
    var black = {0, 0, 0, 255};
    var red = {255, 0, 0, 255};
    var rect = {100, 100, 200, 150};
    KrtWindowClear(window, black);
    KrtDrawRect(window, rect, red);
    // 窗口事件循环
}
```

### KrtDrawCircle

绘制圆形。

```KrtL
function KrtDrawCircle(window: KrtWindow, center: KrtPoint, radius: int, color: KrtColor)
```

**参数:**
- `window`: 窗口句柄
- `center`: 圆心位置
- `radius`: 圆的半径
- `color`: 圆的颜色

**示例:**
```KrtL
var window = KrtWindowCreate(800, 600, "My Window");
if (window != null) {
    KrtWindowShow(window);
    var black = {0, 0, 0, 255};
    var blue = {0, 0, 255, 255};
    var center = {400, 300};
    KrtWindowClear(window, black);
    KrtDrawCircle(window, center, 50, blue);
    // 窗口事件循环
}
```

## 错误处理

### KrtError

报告错误。

```KrtL
function KrtError(code: KrtErrorCode)
```

**参数:**
- `code`: 错误代码

**示例:**
```KrtL
var file = KrtFopen("nonexistent.txt", "r");
if (file == null) {
    KrtError(KRT_ERR_FILE_NOT_FOUND);
}
```

### KrtStrerror

获取错误代码对应的错误消息。

```KrtL
function KrtStrerror(code: KrtErrorCode)
```

**参数:**
- `code`: 错误代码

**返回值:**
- 错误消息字符串

**示例:**
```KrtL
var file = KrtFopen("nonexistent.txt", "r");
if (file == null) {
    var msg = KrtStrerror(KRT_ERR_FILE_NOT_FOUND);
    print(msg);
}
```

### KrtExit

退出程序。

```KrtL
function KrtExit(code: int)
```

**参数:**
- `code`: 退出代码

**示例:**
```KrtL
if (error_occurred) {
    KrtExit(1);
} else {
    KrtExit(0);
}
```

---

*本API参考文档涵盖了Kairote Lang运行时库的主要函数和类型。更多详细信息请参考语言规范和开发者指南。*