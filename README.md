# 导出

## 模块导出

导出到全局环境和导出到模块的区别在于导出的位置。导出到全局环境是直接把欲导出的对象放置到了`_G`中，而导出到模块，则是先根据模块名创建在`_G`中创建对应的表，将此表作为导出的地方。

`LuaMix`默认提供了两个宏，用于指定导出到全局环境或模块。模块名中可以包含`.`，途经的表若没有的话，将自动被创建出来。

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

以下均按照导出到全局环境进行示例。

### 导出为属性

- 支持将C++中的变量导出为属性，此时，在脚本中对属性的赋值操作将修改C++中此变量的值；
- 支持将C++中的变量导出为只读属性，此时，在脚本中对属性赋值的话，将引发异常；
- 支持将C++中的函数以属性的方式导出，函数签名应该符合一般化`get/set`的格式；

```C++
int g_intValue = 666;
int GetIntValue() const { return g_intValue; }
void SetIntValue(int v) { g_intValue = v; }

LUAMIX_GLOBAL_EXPORT(L)
    // 根据变量是否const自动导出，若为const，则自动只读
    .Property("g_intValue", g_intValue)
    // 显式只读导出
    .Property("g_readonly_intValue", g_intValue, true)
    // 将函数以属性的形式导出
    .Property("IntValue", GetIntValue, SetIntValue)
    // 将函数以只读属性的形式导出
    .Property("ReadOnlyIntValue", GetIntValue, nullptr)
    ;
```

### 导出为脚本变量

将一个C++对象导出为脚本变量，则意味着，在脚本中创建了一个变量，该脚本变量中以Lua数据类型保存着C++对象的值。即，在脚本对此变量进行赋值操作，将不会对C++对象有任何影响。注意和属性的区别。

通常会将常量或枚举导出为脚本变量。

也可以将保存全局类实例的变量导出为脚本变量， 只要不是按照`const*`导出，并不影响我们在脚本中修改实例的状态。

```c++
int g_intValue = 666;
enum MyEnum {
	kEnum0 = 0,
	kEnum1,
};

LUAMIX_GLOBAL_EXPORT(L)
	.ScriptVal("g_intValue", g_intValue)
    .ScriptVal("kEnum0", kEnum0)
```

### 导出函数

`LuaMix`支持以下形式的可调用对象导出：

- 普通函数
- `std::function`对象
- lambda对象
- 重载了`operator()`运算符的类对象

同时，对于引用类型的形参，提供了出参支持：基本类型的引用`T&`，类类型指针的引用`C*&`，在非`const`的情况下，将被识别为出参，其修改后的值，将按照出参的顺序，作为额外的返回值返回。

重载的函数，则以别名的形式导出。

```c++
int add_ref(int a, int&b) {
	b++;
	return a + b;
}

int calc_max(int a, int b);
double calc_max(double a, double b, double c);

struct call{
	void operator()(){
		...    
    }
    static void func(){
        ...
    }
}

LUAMIX_GLOBAL_EXPORT(L)
    // 带有出参的普通函数，脚本中可以：local r, b = add_ref(1, 2) --> 4, 3
    .Function("add_ref", add_ref)
    // lambda表达式
    .Function("sum2int", [](int a, int b){ return a + b; })
    // 可调用对象
    .Function("call_obj", call())
    // 静态成员函数
    .Function("call_func", &call::func)
    // 函数重载
    .Function("calc_max_int2", static_cast<int(*)(int, int)>(calc_max))
    .Function("calc_max_double3", static_cast<double(*)(double, double, double)>(calc_max))
    ;
```

## 类导出

LuaMix对类导出提供以下支持：

- 类成员方法导出
- 类静态成员函数导出
- 类成员变量导出
- 类静态成员变量导出
- 以外部函数形式对类进行成员方法和成员变量进行扩充
- 仅支持导出单继承，需要先导出父类，再导出子类

类导出之后，其元表在注册表中维护，同时在全局空间（默认情况）维护一个同名的表，以便于静态成员函数调用和工厂函数调用。

LuaMix提供一对宏开始类的导出：

```c++
#define LUAMIX_CLASS_EXPORT(L, C) ...
#define LUAMIX_CLASS_EXPORT_WITH_BASE(L, C, B) ..
```

### 工厂函数

LuaMix通过工厂函数提供在脚本中创建对象的功能。在脚本中创建的对象，由Lua管理其生命周期，GC时，由注册的回收函数进行回收。

每个类可以有一对默认的工厂和回收函数（需要有默认构造函数）和多对自定义的工厂和回收函数。每个工厂函数创建出来的对象，将由对应的回收函数进行回收。

默认工厂函数中的实现仅仅是`new`和`delete`：

```c++
class Window{
    ...
}
LUAMIX_CLASS_EXPORT(L, Window)
    // 默认工厂和回收函数对，脚本中通过：local pWnd = Window() 创建实例
    .DefaultFactory()
    ;
```

自定义的工厂函数支持函数和仿函，如下：

```c++
class Window{
	static Window *AssignWindow(){
        ...
    }
    static void RecoverWindow(const Window *){
        ...
    }
}
LUAMIX_CLASS_EXPORT(L, Window)
    .DefaultFactory()
    .Factory("AssignWindow", &Window::AssignWindow, &Window::RecoverWindow)
    .Factory("CreateWindow", []{ return new Window; }, [](const Window *wnd){ delete wnd; })
    ;
```

### 成员方法/静态成员方法

除了可导出成员方法外，还可以将一个全局函数作为成员方法导出：

```c++
class Window{
    public:
    	void SetTitle(const std::string &title);
    	void SetTitle(const char *title);
  		static GetWindowCount();
}
const char *GetTitle(const Window* ) const;

LUAMIX_CLASS_EXPORT(L, Window)
    .Method("SetTitle", static_cast<void(Window::*)(const std::string&)>(&Window::SetTitle))
    .Method("SetTitle2", static_cast<void(Window::*)(const char*)>(&Window::SetTitle))
    // 将外部函数导出为成员方法
    .Function("GetTitle", GetTitle)
    // 静态成员方法
    .Function("GetWindowCount", &Window::GetWindowCount)
    ;

```

### 成员变量/静态成员变量导出为属性

类似模块，成员变量以属性的形式导出，以便于脚本中修改对象的状态。

同时支持将一对外部函数以属性的形式注册；

```c++
class Window{
	public int size_;
    static int counter_;
}

int GetWindowSize(const Window* wnd) const;
void SetWindowSize(Window* wnd, int size);

LUAMIX_CLASS_EXPORT(L, Window)
	.Property("Size", &Window::size_)
    // 外部函数形式的属性导出
    .Property("Size2", GetWindowSize, SetWindowSize)
    // 静态成员变量
    .StaticProperty("WindowsCounter", &Window::counter_)
    ;
```

### 继承

有继承关系的类，需要先导出父类，再导出子类。子类中只需要导出自己的成员方法和重载的成员方法即可；

```c++
class Button : public Window {
    ...
}
LUAMIX_CLASS_EXPORT(L, Window)
    .Method(...)
    ;

LUAMIX_CLASS_EXPORT_WITH_BASE(L, Button, Window)
    .Method(...)
    ;
```

