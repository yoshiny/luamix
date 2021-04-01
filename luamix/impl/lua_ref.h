#pragma once

#include "function_proxy.h"

#include <algorithm>

namespace LuaMix::Impl {
	class LuaRef {
	private:
		lua_State* state_;
		int ref_;

	public:
		constexpr LuaRef()
			: state_(nullptr)
			, ref_(LUA_NOREF) {}

		LuaRef(lua_State* L, std::nullptr_t)
			: state_(L)
			, ref_(LUA_REFNIL) {}

		LuaRef(lua_State* L, int index)
			: state_(L) {
			lua_pushvalue(state_, index);
			ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
		}

		LuaRef(const LuaRef& that)
			: state_(that.state_) {
			if (state_) {
				lua_rawgeti(state_, LUA_REGISTRYINDEX, that.ref_);
				ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
			} else {
				ref_ = LUA_NOREF;
			}
		}

		LuaRef& operator = (const LuaRef& that) {
			if (this != &that) {
				if (state_) {
					luaL_unref(state_, LUA_REGISTRYINDEX, ref_);
				}
				state_ = that.state_;
				if (state_) {
					lua_rawgeti(state_, LUA_REGISTRYINDEX, that.ref_);
					ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
				} else {
					ref_ = LUA_NOREF;
				}
			}
			return *this;
		}

		LuaRef& operator = (std::nullptr_t) {
			if (state_) {
				luaL_unref(state_, LUA_REGISTRYINDEX, ref_);
				ref_ = LUA_REFNIL;
			}
			return *this;
		}

		LuaRef(LuaRef&& that) noexcept
			: state_(that.state_)
			, ref_(that.ref_) {
			that.ref_ = LUA_NOREF;
		}

		LuaRef& operator = (LuaRef&& that) noexcept {
			std::swap(state_, that.state_);
			std::swap(ref_, that.ref_);
			return *this;
		}

		~LuaRef() {
			if (state_) {
				luaL_unref(state_, LUA_REGISTRYINDEX, ref_);
			}
		}

	public:
		lua_State* GetState() const {
			return state_;
		}

		int GetRef() const {
			return ref_;
		}

		// 只有在条件表达式中允许编辑器隐式转换，其他情况需要显式转换（通常也不应该用到:-)）
		explicit operator bool() const {
			return ref_ != LUA_NOREF && ref_ != LUA_REFNIL;
		}

		bool IsValid() const {
			return ref_ != LUA_NOREF;
		}

		void Push() const {
			lua_rawgeti(state_, LUA_REGISTRYINDEX, ref_);
		}

	public:
		bool IsTable() const {
			if (ref_ == LUA_NOREF) {
				return false;
			} else if (ref_ == LUA_REFNIL) {
				return false;
			} else {
				Push();
				int t = lua_type(state_, -1);
				lua_pop(state_, 1);
				return t == LUA_TTABLE;
			}
		}

		std::pair<LuaRef, LuaRef> Next(LuaRef it = LuaRef()) const {
			Push();
			if (it) {
				it.Push();
			} else {
				lua_pushnil(state_);
			}
			if (lua_next(state_, -2) == 0) {
				lua_pop(state_, 1);
				return std::make_pair(LuaRef(state_, nullptr), LuaRef(state_, nullptr));
			} else {
				auto val = RefStack(state_);
				auto key = RefStack(state_);
				lua_pop(state_, 1);
				return std::make_pair(key, val);
			}
		}

	public:
		static LuaRef RefStack(lua_State* L) {
			return LuaRef(L);
		}

		static LuaRef RefGlobal(lua_State* L) {
			lua_pushglobaltable(L);
			return RefStack(L);
		}

		static LuaRef RefRegsiter(lua_State* L) {
			return LuaRef(L, LUA_REGISTRYINDEX);
		}

		static LuaRef RefScriptVal(lua_State* L, const char * path) {
			StackGuard _guard(L);
			if (auto pos = strchr(path, '.')) {
				lua_pushglobaltable(L);	// : tb
				int type_top = LUA_TTABLE;
				while (pos) {
					if (type_top != LUA_TTABLE) {
						return LuaRef(L, nullptr);
					}
					lua_pushlstring(L, path, pos - path);	// :tb, key
					type_top = lua_gettable(L, -2);			// :tb, val
					lua_remove(L, -2);						// :val
					if (lua_isnoneornil(L, -1)) {
						return LuaRef(L, nullptr);
					}
					path = pos + 1;
					pos = strchr(path, '.');
				}
				if (type_top != LUA_TTABLE) {
					return LuaRef(L, nullptr);
				}
				lua_pushstring(L, path);	// :tb, key
				lua_gettable(L, -2);		// :tb, val
				return LuaRef(L);
			} else {
				lua_getglobal(L, path);
				return LuaRef(L);
			}
		}

		static LuaRef RefTable(lua_State* L, std::string_view path, bool create_if_no_exist = true) {
			LuaRef cur = RefGlobal(L);
			for (std::size_t s = path.find_first_not_of("."), e = path.find_first_of(".", s); e > s; s = path.find_first_not_of(".", e), e = path.find_first_of(".", s)) {
				auto sub_path = path.substr(s, e - s);
				auto sub = cur.RawGet(sub_path);
				if (sub) {
					if (sub.IsTable()) {
						cur = sub;
					} else {
						return LuaRef(L, nullptr);
					}
				} else if (create_if_no_exist) {
					auto sub = MakeTable(L);
					cur.RawSet(sub_path, sub);
					cur = sub;
				} else {
					return LuaRef(L, nullptr);
				}
			}
			return cur;
		}

		static LuaRef MakeMetatable(lua_State* L, const char *name) {
			luaL_newmetatable(L, name);
			return RefStack(L);
		}

		static LuaRef RefMetatable(lua_State* L, const char* name) {
			luaL_getmetatable(L, name);
			return RefStack(L);
		}

		static LuaRef MakeTable(lua_State* L, int narr = 0, int nrec = 0) {
			lua_createtable(L, narr, nrec);
			return RefStack(L);
		}

		template <typename T>
		static LuaRef MakeFunction(lua_State* L, lua_CFunction fn, T&& obj) {
			if constexpr (std::is_pointer_v<T> && std::is_function_v< std::remove_pointer_t<T> >) {
				lua_pushlightuserdata(L, obj);
			} else {
				pushFunctionObj(L, obj);
			}
			lua_pushcclosure(L, fn, 1);
			return RefStack(L);
		}

		template <typename... Ts>
		static LuaRef MakeCClosure(lua_State *L, lua_CFunction fn, Ts&&... ups) {
			(..., Impl::Push(L, ups));
			lua_pushcclosure(L, fn, sizeof...(ups));
			return RefStack(L);
		}

		static LuaRef MakeCClosure(lua_State *L, lua_CFunction fn, void *p) {
			lua_pushlightuserdata(L, p);
			lua_pushcclosure(L, fn, 1);
			return RefStack(L);
		}

	public:
		LuaRef GetMetatable() const {
			LuaRef meta;
			Push();
			if (lua_getmetatable(state_, -1)) {
				meta = RefStack(state_);
			}
			lua_pop(state_, 1);
			return meta;
		}

		void SetMetatable(LuaRef meta) {
			Push();
			meta.Push();
			lua_setmetatable(state_, -2);
			lua_pop(state_, 1);
		}

	public:
		template <typename K, typename V>
		void RawSet(const K& key, const V& val) {
			Push();
			Impl::Push(state_, key);
			Impl::Push(state_, val);
			lua_rawset(state_, -3);
			lua_pop(state_, 1);
		}

		template <typename K, typename V>
		void Set(const K& key, const V& val) {
			Push();
			Impl::Push(state_, key);
			Impl::Push(state_, val);
			lua_settable(state_, -3);
			lua_pop(state_, 1);
		}

		template <typename V>
		void RawSet(int idx, const V& val) {
			Push();
			Impl::Push(state_, val);
			lua_rawseti(state_, -2, idx);
			lua_pop(state_, 1);
		}

		template <typename V>
		void Set(int idx, const V& val) {
			Push();
			Impl::Push(state_, val);
			lua_seti(state_, -2, idx);
			lua_pop(state_, 1);
		}

		template <typename V>
		void RawSet(void *p, const V& val) {
			Push();
			Impl::Push(state_, val);
			lua_rawsetp(state_, -2, p);
			lua_pop(state_, 1);
		}

		template <typename K, typename V = LuaRef>
		V RawGet(const K& key) const {
			Push();
			Impl::Push(state_, key);
			lua_rawget(state_, -2);
			V val = Impl::Fetch<V>(state_, -1);
			lua_pop(state_, 2);
			return val;
		}

		template <typename K, typename V = LuaRef>
		V Get(const K& key) const {
			Push();
			Impl::Push(state_, key);
			lua_gettable(state_, -2);
			V val = Impl::Fetch<V>(state_, -1);
			lua_pop(state_, 2);
			return val;
		}

		template <typename V = LuaRef>
		V RawGet(int idx) const {
			Push();
			lua_rawgeti(state_, -1, idx);
			V val = Impl::Fetch<V>(state_, -1);
			lua_pop(state_, 2);
			return val;
		}

		template <typename V = LuaRef>
		V Get(int idx) const {
			Push();
			lua_geti(state_, -1, idx);
			V val = Impl::Fetch<V>(state_, -1);
			lua_pop(state_, 2);
			return val;
		}

		template <typename V = LuaRef>
		V RawGet(void* p) const {
			Push();
			lua_rawgetp(state_, -1, p);
			V val = Impl::Fetch<V>(state_, -1);
			lua_pop(state_, 2);
			return val;
		}

	private:
		explicit LuaRef(lua_State* L)
			: state_(L) {
			ref_ = luaL_ref(state_, LUA_REGISTRYINDEX);
		}

	private:
		template <typename T>
		static std::enable_if_t< !std::is_destructible_v<T> || std::is_trivially_destructible_v<T>>
			pushFunctionObj(lua_State* L, const T& obj) {
			void* userdata = lua_newuserdata(L, sizeof(T));
			::new (userdata) T(obj);
		}

		template <typename T>
		static std::enable_if_t< std::is_destructible_v<T> && !std::is_trivially_destructible_v<T> >
			pushFunctionObj(lua_State* L, const T& obj) {
			void* userdata = lua_newuserdata(L, sizeof(T));
			::new (userdata) T(obj);
			lua_newtable(L);
			lua_pushcfunction(L, &destructFunctionObj<T>);
			lua_setfield(L, -2, "__gc");
			lua_setmetatable(L, -2);
		}

		template <typename T>
		static int destructFunctionObj(lua_State* L) {
			try {
				T* obj = static_cast<T*>(lua_touserdata(L, 1));
				obj->~T();  // 注意这里只能析构，不能释放内存，内存块是由lua管理的
				return 0;
			} catch (std::exception& e) {
				return luaL_error(L, "%s", e.what());
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 特化 LuaRef
	template <>
	struct _type_mix_spec<LuaRef> {
		using ArgHoldType = LuaRef;

		static void Push(lua_State* L, const LuaRef& value) {
			if (value.IsValid()) {
				value.Push();
			} else {
				lua_pushnil(L);
			}
		}

		template <bool CHECK = false>
		static LuaRef Fetch(lua_State* L, int index) {
			return lua_isnone(L, index) ? LuaRef() : LuaRef(L, index);
		}
	};
}
