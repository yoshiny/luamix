#pragma once

#include "luamix.h"

namespace LuaMix {
	class LuaState {
	public:
		LuaState() 
			: view_(false)
		{
			state_ = luaL_newstate();
			luaL_openlibs(state_);
		}

		LuaState(lua_State *L)
			: view_(true)
			, state_(L)
		{}
		
		~LuaState() {
			if (state_ && !view_) {
				lua_close(state_);
			}
		}

	public:
		operator lua_State* () const {
			return state_;
		}

		lua_State* GetState() const {
			return state_;
		}

		explicit operator bool() const {
			return state_ != nullptr;
		}

		bool IsView() const {
			return view_;
		}

	public:
		void DoFile(const std::string &file_path) {
			StackGuard _guard(state_);
			lua_pushcfunction(state_, LuaException::StackTraceback);
			if (luaL_dofile(state_, file_path.c_str())) {
				throw LuaException(state_);
			}
		}

		void DoString(const std::string& script) {
			StackGuard _guard(state_);
			lua_pushcfunction(state_, LuaException::StackTraceback);
			if (luaL_dostring(state_, script.c_str())) {
				throw LuaException(state_);
			}
		}

		// 系统集成时，总是应该在此处捕获错误信息，并记录日志，避免让上层逻辑代码处理
		template <typename... Rs, typename... Ts>
		decltype(auto) Call( const char *func_path, Ts&&... args ) {
			ScriptCall<Rs...> sc(state_, func_path);
			auto rst = sc.Call(std::forward<Ts>(args)...);
			if (!rst) {
				std::cout << sc.ErrMsg() << std::endl;
			}
			return rst;
		}

		template <typename... Rs, typename... Ts>
		decltype(auto) SelfCall(const char* func_path, const char *method, Ts&&... args) {
			ScriptCall<Rs...> sc(state_, func_path);
			auto rst = sc.SelfCall(method, std::forward<Ts>(args)...);
			if (!rst) {
				std::cout << sc.ErrMsg() << std::endl;
			}
			return rst;
		}

	private:
		lua_State* state_;
		bool view_;
	};
}