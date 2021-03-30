#include <iostream>
#include <string>
#include <array>
#include <tuple>
#include <vector>

#include "luamix/luamix.h"
#include "luamix/lua_state.h"
#include "luamix/vector_mix.h"


struct Window {
	std::string text_;

	virtual std::string GetText() const noexcept {
		return text_;
	}

	int size_{ 0 };
};

struct Button : Window {
	std::string GetText() const noexcept {
		return "[button]" + text_;
	}
};


int a = 111;

int b = 0;
int c = 999;

int add(int p1, int& p2) {
	p2++;
	return p1 + p2;
}

int main() {
	LuaMix::LuaState state;

	//////////////////////////////////////////////////////////////////////////




	LUAMIX_CLASS_EXPORT(state, Window)
.DefaultFactory()
		.Method("GetText", &Window::GetText)
			.Function("add", add)
			.Property("Text", &Window::text_);

		LUAMIX_CLASS_EXPORT_WITH_BASE(state, Button, Window)
			.Factory("new", [] { return new Button; }, [](Button* sw) {
			std::cout << sw->GetText() << " gced.\n";
			delete sw; })
			.Method("GetText", &Window::GetText);

			LUAMIX_VECTOR_SUPPORT(state, int);
			LUAMIX_VECTOR_SUPPORT(state, Window*);
			//Window w;

			std::string str("asd");
			const std::string& str2 = str;


			Button btn;

			std::vector<int> vi{ 1, 2, 3 };

			std::vector<Window*> vw;

			LUAMIX_MODULE_EXPORT(state, "test")
				.ScriptVal("vi", (&vi))
				.ScriptVal("vw", (&vw))
				.ScriptVal("pWnd", dynamic_cast<Window *>(&btn))
				.ScriptVal("pBtn", &btn)
				.ScriptVal("a", a)

				.ScriptVal("str2", str2)

				//.ScriptVal("w", const_cast<const Window *>(&w))
				.Property("c", []() {return c; }, nullptr)
				.Function("add", &add)
				.Property("b", []() {return b; }, [](int p) {b = p; });

			//////////////////////////////////////////////////////////////////////////
			if (luaL_dofile(state, "playground.lua")) {
				std::cout << lua_tostring(state, -1) << std::endl;
				lua_pop(state, 1);
			}

			//int a = 88, b = 99;
			//
			//if (auto rst = state.SelfCall<int, int>("solar", "Dump", 1)) {
			//	std::tie(a, b) = rst.value();
			//}

			//std::cout << a << std::endl;
			//std::cout << b << std::endl;

			if (auto rst = state.Call<Window *>("Window.new")) {
				std::cout << rst.value() << std::endl;
			}

			return 0;
}