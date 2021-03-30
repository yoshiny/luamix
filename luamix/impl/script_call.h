#pragma once

#include <optional>

#include "lua_ref.h"

namespace LuaMix::Impl {
	template <typename... Rs>
	struct ScriptCallImpl {
		using ReturnType = std::optional<std::tuple<Rs...>>;

		template <typename... Ps>
		static ReturnType Call(lua_State* L, int fn_ref, Ps&&... args) {
			StackGuard _guard(L);
			lua_pushcfunction(L, LuaException::StackTraceback);
			lua_rawgeti(L, LUA_REGISTRYINDEX, fn_ref);
			(Push(L, args), ...);
			if (lua_pcall(L, sizeof...(Ps), sizeof...(Rs), -int(sizeof...(Ps) + 2)) != LUA_OK) {
				throw LuaException(L);
			} else {
				int index = 0;
				std::tuple<Rs...> rets;
				std::apply([L](auto &... ret) {
					int index = sizeof...(Rs);
					((ret = Fetch<Rs>(L, -(index--))), ...);
					}, rets);
				return rets;
			}
		}

		template <typename... Ps>
		static ReturnType SelfCall(lua_State *L, int callee_ref, const char *method, Ps&&... args) {
			StackGuard _guard(L);
			lua_rawgeti(L, LUA_REGISTRYINDEX, callee_ref); // :..., callee
			lua_pushcfunction(L, LuaException::StackTraceback); // :..., callee, trace
			lua_getfield(L, -2, method);	// :..., callee, trace, method
			lua_pushvalue(L, -3); // :..., callee, trace, method, callee
			(Push(L, args), ...); // :..., callee, trace, method, callee, args...
			if (lua_pcall(L, sizeof...(Ps) + 1, sizeof...(Rs), -int(sizeof...(Ps) + 2 + 1)) != LUA_OK) {
				throw LuaException(L);
			} else {
				int index = 0;
				std::tuple<Rs...> rets;
				std::apply([L](auto &... ret) {
					int index = sizeof...(Rs);
					((ret = Fetch<Rs>(L, -(index--))), ...);
					}, rets);
				return rets;
			}
		}
	};

	template <typename R>
	struct ScriptCallImpl<R> {
		using ReturnType = std::optional<R>;

		template <typename... Ps>
		static std::optional<R> Call(lua_State* L, int fn_ref, Ps&&... args) {
			StackGuard _guard(L);
			lua_pushcfunction(L, LuaException::StackTraceback);
			lua_rawgeti(L, LUA_REGISTRYINDEX, fn_ref);
			(Push(L, args), ...);
			if (lua_pcall(L, sizeof...(Ps), 1, -int(sizeof...(Ps) + 2)) != LUA_OK) {
				throw LuaException(L);
			} else {
				return Fetch<R>(L, -1);
			}
		}

		template <typename... Ps>
		static std::optional<R> SelfCall(lua_State* L, int callee_ref, const char* method, Ps&&... args) {
			StackGuard _guard(L);
			lua_rawgeti(L, LUA_REGISTRYINDEX, callee_ref); // :..., callee
			lua_pushcfunction(L, LuaException::StackTraceback); // :..., callee, trace
			lua_getfield(L, -2, method);	// :..., callee, trace, method
			lua_pushvalue(L, -3); // :..., callee, trace, method, callee
			(Push(L, args), ...); // :..., callee, trace, method, callee, args...
			if (lua_pcall(L, sizeof...(Ps) + 1, 1, -int(sizeof...(Ps) + 2 + 1)) != LUA_OK) {
				throw LuaException(L);
			} else {
				return Fetch<R>(L, -1);
			}
		}
	};

	template <typename... Rs>
	class ScriptCall {
	public:
		ScriptCall(lua_State *L, const char *callee_path)
			: state_(L)
			, callee_ref_(LUA_NOREF)
		{
			callee_ref_ = MakeScriptValRef(state_, callee_path);
		}

		ScriptCall(const ScriptCall<Rs...>& that)
			: state_(that.state_)
		{
			if (state_) {
				lua_rawgeti(state_, LUA_REGISTRYINDEX, that.callee_ref_);
				callee_ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
			} else {
				callee_ref_ = LUA_NOREF;
			}
		}

		ScriptCall<Rs...>& operator = (const ScriptCall<Rs...>& that) {
			if (this != &that) {
				if (state_) {
					luaL_unref(state_, LUA_REGISTRYINDEX, callee_ref_);
				}
				state_ = that.state_;
				if (state_) {
					lua_rawgeti(state_, LUA_REGISTRYINDEX, that.callee_ref_);
					callee_ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
				} else {
					callee_ref_ = LUA_NOREF;
				}
			}
			return *this;
		}

		ScriptCall(ScriptCall<Rs...>&& that) noexcept
			: state_(that.state_)
			, callee_ref_(that.callee_ref_)
			, errmsg_(std::move(that.errmsg_))
		{
			that.callee_ref_ = LUA_NOREF;
		}

		ScriptCall<Rs...>& operator = (ScriptCall<Rs...>&& that) noexcept {
			std::swap(state_, that.state_);
			std::swap(callee_ref_, that.callee_ref_);
			std::swap(errmsg_, that.errmsg_);
			return *this;
		}

		~ScriptCall() {
			luaL_unref(state_, LUA_REGISTRYINDEX, callee_ref_);
		}

	public:
		using ReturnType = typename ScriptCallImpl<Rs...>::ReturnType;

		template <typename... Ps>
		ReturnType Call(Ps&&... args) {
			try {
				return ScriptCallImpl<Rs...>::Call(state_, callee_ref_, std::forward<Ps>(args)...);
			} catch (const LuaException& e) {
				errmsg_ = e.what();
				return std::nullopt;
			}
		}

		template <typename... Ps>
		ReturnType SelfCall(const char *method, Ps&&... args) {
			try {
				return ScriptCallImpl<Rs...>::SelfCall(state_, callee_ref_, method, std::forward<Ps>(args)...);
			} catch (const LuaException& e) {
				errmsg_ = e.what();
				return std::nullopt;
			}
		}

	public:
		const std::string& ErrMsg() const {
			return errmsg_;
		}

	private:
		lua_State* state_;
		int callee_ref_;
		std::string errmsg_;
	};
}