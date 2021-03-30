#pragma once

#include <vector>
#include <cctype>

#include "luamix.h"

#define LUAMIX_VECTOR_SUPPORT(L, T)	LuaMix::VectorMix<T>::Support(L, #T);

namespace LuaMix {
	template <typename T>
	struct VectorMix {
		static void Support(lua_State *L, const char *name) {
			// name剔除一下空格，避免类似*号停靠带来的问题
			std::vector<char> fname(strlen(name) + 1, 0);
			auto it = fname.begin();
			for (auto pos = 0; pos < fname.size() - 1; ++pos) {
				if (auto ch = static_cast<unsigned char>(*(name + pos)); !std::isspace(ch)) {
					*it = ch; ++it;
				}
			}

			using VT = std::vector<T>;

			std::string reg_name = std::string("std::vector<") + fname.data() + ">";
			std::string const_name = "const " + reg_name;
			auto cm = ClassDef<VT>(L, reg_name.c_str(), const_name.c_str());

			// 精准匹配重载的方法
			cm.Method("empty", static_cast<bool(VT::*)()const>(&VT::empty));
			cm.Method("size", static_cast<typename VT::size_type(VT::*)()const>(&VT::size));
			cm.Method("max_size", static_cast<typename VT::size_type(VT::*)()const>(&VT::max_size));
			cm.Method("reserve", static_cast<void(VT::*)(typename VT::size_type)>(&VT::reserve));
			cm.Method("capacity", static_cast<typename VT::size_type(VT::*)()const>(&VT::capacity));
			cm.Method("shrink_to_fit", static_cast<void(VT::*)()>(&VT::shrink_to_fit));
			cm.Method("clear", static_cast<void(VT::*)()>(&VT::clear));
			cm.Method("resize", static_cast<void(VT::*)(typename VT::size_type)>(&VT::resize));
			cm.Method("pop_back", static_cast<void(VT::*)()>(&VT::pop_back));
			cm.Method("push_back", static_cast<void(VT::*)(const T&val)>(&VT::push_back));

			// 以首个参数是self对象的方式，扩展类的成员方法
			// 这些函数直接实现成静态成员函数效率会更高
			auto resize_with = [](VT *vec, int count, const T& val) {
				vec->resize(count, val);
			};
			// 这里给一个别名，以绕过resize函数的重载
			cm.Function("resize_with", resize_with);

			auto swap_with = [](VT* self, VT* other) {
				self->swap(*other);
			};
			cm.Function("swap", swap_with);

			auto insert_with = [](VT* vec, int pos, const T& val) {
				auto it = vec->begin();
				std::advance(it, pos);
				vec->insert(it, val);
			};
			cm.Function("insert", insert_with);

			auto erase_with = [](VT* vec, int pos) {
				auto it = vec->begin();
				std::advance(it, pos);
				if (vec->end() == vec->erase(it)) {
					return -1;
				} else {
					return pos;
				}
			};
			cm.Function("erase", erase_with);

			auto get_with = [](VT* vec, int pos) -> T& {
				return vec->at(pos);
			};
			cm.Function("get", get_with);

			auto set_with = [](VT* vec, int pos, const T& val){
				vec->at(pos) = val;
			};
			cm.Function("set", set_with);

			cm.CClosure("ipairs", &ipairs, const_name);

			cm.Factory("new", [] { return new std::vector<T>; }, [](std::vector<T>* v) { delete v; });
		}

		static int ipairsaux(lua_State* L) {
			lua_Integer i = luaL_checkinteger(L, 2) + 1;
			lua_pushinteger(L, i);
			auto vec = Impl::Fetch<const std::vector<T>*, true>(L, 1);
			if (i >= (lua_Integer)vec->size()) {
				lua_pushnil(L);
				return 1;
			} else {
				Impl::Push(L, vec->at(i));
				return 2;
			}
		}

		static int ipairs(lua_State* L) {
			const char* meta = lua_tostring(L, lua_upvalueindex(1));
			lua_pushcfunction(L, &ipairsaux);	// 真正的迭代器
			lua_pushvalue(L, 1);	// 以vector本身作为不变量
			lua_pushinteger(L, -1);	// 迭代子初始值
			return 3;
		}
	};
}