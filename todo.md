## 高优先级（标准库核心依赖）
1. default(T) - 泛型默认值
2. Func<T, TResult> - 委托类型
3. Action <t/> - 委托类型
4. Predicate <t/> - 委托类型
5. Converter<TInput, TOutput> - 委托类型
6. delegate - 委托声明
7. is - 类型检查
8. as - 类型转换
9. 显式类型转换 - IComparable <t>(x)</t>
10. 操作符重载 - op_Addition 等
11. lock - 线程同步
12. yield return/break - 延迟枚举
## 中优先级
13. using - 资源管理
14. throw; - 重新抛出
15. 可选参数 - default value
17. 可空类型 - int32?
18. 空合并 - ??
19. 空条件 - ?.
20. LINQ orderby - 排序
21. LINQ group by - 分组
22. LINQ let - 变量绑定
23. 属性简写 - { get; set; }
24. 泛型约束 - where T : class
25. 泛型静态字段
## 低优先级
26. 异常过滤器 - catch when
27. 字符串插值 - $"..."
28. 元组 - (int, string)
29. 模式匹配
30. 指针类型 - int32*
31. fixed 语句
32. sizeof
33. stackalloc
34. async/await