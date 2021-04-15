#include <iostream>
#include <string>
#include <array>
#include <tuple>
#include <vector>

#include "luamix/luamix.h"
#include "luamix/lua_state.h"
#include "luamix/vector_mix.h"

class Window {
public:
	float GetSize() const noexcept {
		return size_;
	}

	void SetSize(float size) {
		size_ = size;
	}

	std::string GetTitle() const {
		return title_;
	}

	virtual void SetTitle(const std::string &title) {
		title_ = title;
	}

	virtual void SetTitle(const char *title) {
		title_ = title;
	}

	static Window *CreateWnd() {
		std::cout << "Window *CreateWnd called.\n";
		return new Window;
	}

	static void DestoryWnd(const Window *wnd) {
		std::cout << "Window DestoryWnd called.\n";
		delete wnd;
	}

	static void ClearWndTitle(Window *wnd) {
		wnd->title_.clear();
	}

	static int GetWindowCounter() { return counter_; }

public:
	static int counter_;

public:
	float size_;
	std::string title_;
};

int Window::counter_ = 999;

class Button : public Window {
public:
	virtual void SetTitle(const char *title) override {
		title_ = "[Button]";
		title_ += title;
	}

	virtual void SetTitle(const std::string &title) override {
		title_ = "[Button]" + title;
	}
};

int g_IntValue = 999;

const int g_ConstIntVal = 888;

int add_ref(int a, int&b) {
	b++;
	return a + b;
}

template <typename... Ts>
void print_any( Ts... args ) {
	( (std::cout << args << std::ends), ... );
	if constexpr (sizeof...(args) > 0) {
		std::cout << std::endl;
	}
}

Window g_wnd;

const Window * g_cpwnd = &g_wnd;
Window * g_pwnd = &g_wnd;

bool fetch_window(const Window *& wnd ) {
	wnd = &g_wnd;
	return true;
}

enum MyEnum {
	kEnum0 = 0,
	kEnum1,
	kEnum2,
};

int main() {
	LuaMix::LuaState state;
	//////////////////////////////////////////////////////////////////////////
	// 全局注册
	LUAMIX_GLOBAL_EXPORT(state)
		.Function("add_ref", add_ref)
		.Function("print1", print_any<int>)
		.Function("print2", print_any<int, int>)
		.Function("fetch_window", fetch_window)
		.Property("IntValue", []() { return g_IntValue; }, [](int v) { g_IntValue = v; })
		.Property("ReadOnlyIntValue", []() { return g_IntValue; }, nullptr)
		.ScriptVal("kEnum0", kEnum0)
		.ScriptVal("kEnum1", kEnum1)
		.Property("g_pwnd", g_pwnd)
		.Property("g_cpwnd", g_cpwnd)
		.Property("g_IntValue", g_IntValue, true)
		.Property("g_ConstIntValue", g_ConstIntVal)
		;

	// 模块注册
	LUAMIX_MODULE_EXPORT(state, "cpp.test")
		.Property("IntValue", []() { return g_IntValue; }, [](int v) { g_IntValue = v; })
		;
		
	LUAMIX_MODULE_EXPORT(state, LuaMix::LuaRef::RefTable(state, "cpp.test"))
		.ScriptVal("kEnum1", kEnum1)
		;

	//////////////////////////////////////////////////////////////////////////
	// 类注册
	LUAMIX_CLASS_EXPORT(state, Window)
		.Method("GetSize", &Window::GetSize)
		.Method("SetSize", &Window::SetSize)
		.Method("GetTitle", &Window::GetTitle)
		.Method("SetTitle", static_cast<void(Window::*)(const std::string&)>(&Window::SetTitle))
		.Method("SetTitle2", static_cast<void(Window::*)(const char*)>(&Window::SetTitle))
		.DefaultFactory()
		.Factory("CreateWnd", &Window::CreateWnd, &Window::DestoryWnd)
		.Factory("TraceCreate", [] { Window * w = new Window; std::cout << w << " created from script.\n"; return w; }, [](const Window *p) {std::cout << p << " auto gced.\n"; delete p; })
		.Function("ClearWndTitle", &Window::ClearWndTitle)
		.Property("Size", &Window::size_)
		.Property("Title", &Window::title_)
		.Property("Title2", [](const Window * w) { return w->GetTitle(); }, [](Window * w, const char *t) { w->SetTitle(t); })
		.Property("ReadOnlyTitle", [](const Window * w) { return w->GetTitle(); }, nullptr)
		.StaticProperty("WindowCounter", Window::counter_)
		.Function("GetWindowCounter", &Window::GetWindowCounter)
		;

	LUAMIX_VECTOR_SUPPORT(state, Window*);

	LUAMIX_CLASS_EXPORT_WITH_BASE(state, Button, Window)
		.Method("SetTitle", static_cast<void(Button::*)(const std::string&)>(&Button::SetTitle))
		.Method("SetTitle2", static_cast<void(Button::*)(const char*)>(&Button::SetTitle))
		.Method("SetTitle3", static_cast<void(Window::*)(const char*)>(&Window::SetTitle))
		;

	LUAMIX_VECTOR_SUPPORT(state, Button*);

	std::vector<Window *> vws;
	Button vb;
	Window vw;
	LUAMIX_GLOBAL_EXPORT(state)
		.ScriptVal("g_Windows", &vws)
		.ScriptVal("g_Button", &vb)
		.ScriptVal("g_ButtonAsWindow", dynamic_cast<Window *>(&vb))
		.ScriptVal("g_ConstWnd", const_cast<const Window *>(&vw))
		;

	//////////////////////////////////////////////////////////////////////////
	if (luaL_dofile(state, "playground.lua")) {
		std::cout << lua_tostring(state, -1) << std::endl;
		lua_pop(state, 1);
	}

	// 脚本函数调用测试
	std::cout << "Calling Script Function:-------------------------\n";
	if (auto rst = state.Call<Window *>("Window")) {
		std::cout << "Create Window From Script:" << rst.value() << std::endl;
	}

	if (auto rst = state.Call<std::string>("test.SingleReturn", "string to retrieve")) {
		std::cout << "Retrieve String From Script Call<test.SingleReturn>:" << rst.value() << std::endl;
	}

	if (auto rst = state.SelfCall<double, int, float>("test", "MultiReturn", 88)) {
		auto[a, i, b] = rst.value();
		std::cout << "Retrieve Multi Value From Script SelfCall<test::MultiReturn>:" << a << std::ends << i << std::ends << b << std::endl;

		double c = 999.0; int j = 0; float d = 999.999f;
		std::tie(c, j, d) = rst.value();
		std::cout << "Retrieve Multi Value From Script SelfCall<test::MultiReturn>:" << c << std::ends << j << std::ends << d << std::endl;
	}

	if (auto rst = state.Call<int, float, std::string>("ScriptMultiPassAndRet", 999, "string", 88.88)) {
		auto[i, s, f] = rst.value();
		std::cout << i << std::ends << s << std::ends << f << std::endl;
	}

	return 0;
}