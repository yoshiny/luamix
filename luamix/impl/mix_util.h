#pragma once

#include "lualib.h"
#include "lauxlib.h"

#include <stdexcept>

namespace LuaMix::Impl
{
	//////////////////////////////////////////////////////////////////////////
	// 栈保护
	struct StackGuard {
		int top_;
		lua_State* state_;

		StackGuard(lua_State* L)
			: state_(L)
			, top_(lua_gettop(L)) {}

		~StackGuard() {
			lua_settop(state_, top_);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 异常
	class LuaException : public std::exception {
	public:
		explicit LuaException(lua_State* L) noexcept {
			if (lua_gettop(L) > 0) {
				what_ = lua_tostring(L, -1);
			} else {
				what_ = "unknown error";
			}
		}

		explicit LuaException(const char* msg) noexcept
			: what_(msg) {}

		explicit LuaException(const std::string& msg) noexcept
			: what_(msg) {}

		const char* what() const noexcept {
			return what_.c_str();
		}

	public:
		// 调用栈追溯
		static int StackTraceback(lua_State* L) {
			auto msg = lua_tostring(L, 1);
			if (!msg) {
				if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
					// 如果错误对象可以通过元方法tostring，则视其结果为错误信息
					msg = lua_tostring(L, -1);
				} else {
					msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
				}
			}
			// 追加调用栈信息
			luaL_traceback(L, L, msg, 1);
			return 1;
		}

	private:
		std::string what_;
	};

	//////////////////////////////////////////////////////////////////////////
	// 工具函数
	
	inline constexpr const char* LUAMIX_KEY_INIT = "luamix_init";
	inline constexpr const char* LUAMIX_KEY_COLLECT = "luamix_collect";

	inline int MakeScriptValRef(lua_State *L, const char *path) {
		StackGuard _guard(L);
		if (auto pos = strchr(path, '.')) {
			lua_pushglobaltable(L);	// :tb
			int tt = LUA_TTABLE;
			while (pos) {
				if (LUA_TTABLE != tt) {
					lua_pushnil(L);
					return luaL_ref(L, LUA_REGISTRYINDEX);
				}
				lua_pushlstring(L, path, pos - path); // :tb, key
				tt = lua_gettable(L, -2);	// :tb, val
				lua_remove(L, -2);	// :val
				if (lua_isnoneornil(L, -1)) {
					return luaL_ref(L, LUA_REGISTRYINDEX);
				}
				path = pos + 1;
				pos = strchr(path, '.');
			}
			if (LUA_TTABLE != tt) {
				lua_pushnil(L);
				return luaL_ref(L, LUA_REGISTRYINDEX);
			}
			lua_pushstring(L, path); // :tb, key
			lua_gettable(L, -2); // :tb, val
		} else {
			lua_getglobal(L, path);
		}
		return luaL_ref(L, LUA_REGISTRYINDEX);
	}

	// 接管userdata的生命周期
	inline int TakeOwnership(lua_State *L) {
		if (LUA_TUSERDATA != lua_type(L, 1)) {
			return 0;
		}
		void* ud = *((void**)lua_touserdata(L, 1));
		lua_pushstring(L, LUAMIX_KEY_COLLECT);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_pushlightuserdata(L, ud);
		lua_rawsetp(L, -2, ud);
		return 0;
	}

	// 释放userdata生命周期管理
	inline int ReleaseOwnership(lua_State *L) {
		if (LUA_TUSERDATA != lua_type(L, 1)) {
			return 0;
		}
		void* ud = *((void**)lua_touserdata(L, 1));
		lua_pushstring(L, LUAMIX_KEY_COLLECT); // :ud, "collect"
		lua_rawget(L, LUA_REGISTRYINDEX);	// :ud, collect
		if (LUA_TNIL != lua_rawgetp(L, -1, ud)) { // :ud, collect, val
			lua_pushnil(L);	// :ud, collect, val, nil
			lua_rawsetp(L, -3, ud);
		}
		return 0;
	}

	// 获取userdata的peer表
	inline int GetPeer(lua_State *L) {
		if (lua_type(L, 1) != LUA_TUSERDATA) {
			lua_pushnil(L);
		} else {
			lua_getuservalue(L, 1);
		}
		return 1;
	}

	// 设置userdata的peer表
	inline int SetPeer(lua_State* L) {
		luaL_checktype(L, 1, LUA_TUSERDATA);
		luaL_checktype(L, 2, LUA_TTABLE);
		lua_settop(L, 2);
		lua_setuservalue(L, 1);
		return 1;
	}
}