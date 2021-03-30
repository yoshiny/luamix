#pragma once

#include "impl/mix_util.h"
#include "impl/type_mix.h"
#include "impl/function_proxy.h"
#include "impl/lua_ref.h"
#include "impl/meta_mix.h"
#include "impl/script_call.h"


namespace LuaMix {
	using LuaRef = Impl::LuaRef;
	using StackGuard = Impl::StackGuard;
	using LuaException = Impl::LuaException;

	template <typename... Rs>
	using ScriptCall = Impl::ScriptCall<Rs...>;

	// 模块定义
	Impl::ModuleMeta ModuleDef(lua_State *L, const char* name) {
		auto md = LuaRef::RefTable(L, name, true);
		if (!md) {
			std::string errmsg = "error module path: ";
			throw std::runtime_error(errmsg + name);
		}
		return Impl::ModuleMeta(md);
	}

	Impl::ModuleMeta ModuleDef(lua_State* L) {
		return Impl::ModuleMeta(LuaRef::RefGlobal(L));
	}

	// 类定义
	template <typename C, typename B = void>
	Impl::ClassMeta<C> ClassDef(lua_State* L, const char* class_name, const char* const_name, LuaRef export_to = LuaRef()) {
		if (!export_to) {
			export_to = LuaRef::RefGlobal(L);
		}
		Impl::ClassMeta<C> cm(L, class_name, const_name);
		if constexpr (!std::is_same_v<B, void>) {
			cm.Inhertance<B>();
		}

		auto tb = LuaRef::MakeTable(L);
		tb.SetMetatable(cm.RefClassMetatable());

		export_to.RawSet(class_name, tb);
		return cm;
	}
}

// 便捷宏
#define LUAMIX_MODULE_EXPORT(L, M)	LuaMix::ModuleDef(L, M)
#define LUAMIX_GLOBAL_EXPORT(L)	LuaMix::ModuleDef(L)

#define LUAMIX_CLASS_EXPORT(L, C)	LuaMix::ClassDef<C>(L, #C, "const "#C)
#define LUAMIX_CLASS_EXPORT_WITH_BASE(L, C, B)	LuaMix::ClassDef<C, B>(L, #C, "const "#C)


