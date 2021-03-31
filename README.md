# 使用

## 导出

### 模块导出

```c++
int g_IntVal = 999;
int add(int a, int b);
void print(int a);
void print(const char * s)
int add_ref(int a, float& b);
int add_ref2(int a, const float& b);

enum MyEnum {
	kEnum0 = 0,
	kEnum1,
	kEnum2,
};

lua_State *L = ...;

// 导出到全局空间
LUAMIX_GLOBAL_EXPORT(L)
    .Function("add", add)
    .Function("add_ref", add_ref) // 出参支持
    .Function("add_ref2", add_ref2) // 常量引用，并不是出参
    .Function("print", static_cast<void(*)(int)>(print)) // 重载函数需要显式指定欲导出的韩式
    .Function("print2", static_cast<void(*)(const char * s)>(print)) // 以别名的形式导出另一个重载定义
    .ScriptVal("kEnum0", kEnum0)	// 导出成脚本变量
    .Property("IntValue", []() { return g_IntValue; }, [](int v) { g_IntValue = v; }) // 导出成属性
    .Property("ReadOnlyIntValue", []() { return g_IntValue; }, nullptr) // 导出成只读属性
    ;

// 导出到指定表中
LUAMIX_MODULE_EXPORT(L, LuaMix::RefTable(L, "modulename"))
    .Function ... // 这里就和导出到全局空间一样了




```



## 脚本函数调用

# 扩充

