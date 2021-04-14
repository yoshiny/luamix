# 导出

## 模块导出

导出到全局环境和模块的区别在于对导出目标的引用：

```c++
lua_State *L;
// 导出到全局环境
LUAMIX_GLOBAL_EXPORT(L)
    .Function(...)
    ;
// 导出到模块，模块路径为：_G.module_name
LUAMIX_MODULE_EXPORT(L, "module_name")
    .Function(...)
    ;
// 导出到模块，模块路径为：_G.test.module_name，途径的table会自动创建
LUAMIX_MODULE_EXPORT(L, "test.module_name")
    .Function(...)
    ;
// 导出到模块, 模块路径为：_G.test.module_name，途径的table会自动创建
LUAMIX_MODULE_EXPORT(L, LuaMix::LuaRef::RefTable(state, "test.module_name"))
    .Function(...)
    ;
```

以下以导出到全局环境为例，展示不同成员的导出：

导出为脚本变量

```c++
int g_IntVal = 999;
enum MyEnum {
	kEnum0 = 0,
	kEnum1,
};

LUAMIX_GLOBAL_EXPORT(L)
	.ScriptVal("g_IntVal", g_IntVal)
    .ScriptVal("kEnum0", kEnum0)
```

函数

```c++
int get_int(){ return g_IntValue; }
void set_int(int v){ g_IntValue = v; }
int add_ref(int a, int&b) {
	b++;
	return a + b;
}

LUAMIX_GLOBAL_EXPORT(L)
    .Function("get_int", get_int)
    .Function("set_int", set_int)
    .Function("add_ref", add_ref)
    .Function("sum2int", [](int a, int b){ return a + b; })
```

属性：

```c++
const int g_ConstIntVal = 888;

LUAMIX_GLOBAL_EXPORT(L)
	.Property("IntVal", g_IntVal)			// 可读写属性
	.Property("ReadOnlyIntVal", g_IntVal, true)	// 只读属性
    .Property("IntVal2", []() { return g_IntValue; }, [](int v) { g_IntValue = v; }) // 自定义属性行为
    .Property("ReadOnlyIntVal2", []() { return g_IntValue; }, nullptr) // 自定义只读属性行为
    .Property("ConstIntVal", g_ConstIntVal) // 常量自动自读
    ;
```

## 类导出

有类如下：

```c++
class Window{
    public:
    	virtual void SetTitle(const std::string &title);
    	virtual void SetTitle(const char *title);
    private:
    	std::string title_;
}
```



```c++

class Window{
    ...
}
class Button : public Window{
    ...
}

// 先导出基类
LUAMIX_CLASS_EXPORT(L, Window)
    // 默认工厂及回收
    .DefaultFactory()
    // 自定义工厂及回收
    .Factory("TraceCreate", [] { Window * w = new Window; std::cout << w << " created from script.\n"; return w; }, []( Window *p) {std::cout << p << " auto gced.\n"; delete p; })
    // 成员方法
	.Method("GetSize", &Window::GetSize)
	.Method("SetSize", &Window::SetSize)
    // 重载的成员方法
    .Method("SetTitle", static_cast<void(Window::*)(const std::string&)>(&Window::SetTitle))
	.Method("SetTitle2", static_cast<void(Window::*)(const char*)>(&Window::SetTitle))
    // 静态成员函数
	.Function("GetWindowCounter", &Window::GetWindowCounter)
    ;
    
    
```