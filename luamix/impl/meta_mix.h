#pragma once

#include <stdexcept>

#include "lua_ref.h"

namespace LuaMix::Impl {
	//////////////////////////////////////////////////////////////////////////
	// 全局方法
	struct MixMetaEvent {
		static void Init(lua_State* L) {
			StackGuard _guard(L);

			lua_pushstring(L, LUAMIX_KEY_INIT);		// :key
			lua_rawget(L, LUA_REGISTRYINDEX);		// :flag
			if (!lua_isboolean(L, -1)) {
				lua_pushstring(L, LUAMIX_KEY_INIT);	// :nil, key
				lua_pushboolean(L, 1);				// :nil, key, flag
				lua_rawset(L, LUA_REGISTRYINDEX);	// :nil

				// 接管生命周期的ud集合
				lua_pushstring(L, LUAMIX_KEY_COLLECT);	// :nil, key
				lua_newtable(L);						// :nil, key, tb
				lua_rawset(L, LUA_REGISTRYINDEX);		// :nil

				// 工具函数
				static const luaL_Reg util_funcs[] = {
					{"TakeOwnership", TakeOwnership},
					{"ReleaseOwnership", ReleaseOwnership},
					{"GetPeer", GetPeer},
					{"SetPeer", SetPeer},
					{NULL, NULL}
				};
				luaL_newlib(L, util_funcs);
				lua_setglobal(L, "LuaMix");
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 模块元方法
	struct ModuleMetaEvent {
		static int index(lua_State* L) {
			lua_getmetatable(L, 1);		// : tb, key, mt
			lua_pushstring(L, MetaKeyGet); // : tb, key, mt, ".get"
			lua_rawget(L, -2);			// : tb, key, mt, val
			lua_pushvalue(L, 2);		// : tb, key, mt, val, key
			lua_rawget(L, -2);			// : tb, key, mt, val
			if (lua_isfunction(L, -1)) {
				lua_call(L, 0, 1);
			}
			return 1;
		}

		static int newindex(lua_State* L) {
			lua_getmetatable(L, 1);			// : tb, key, val, mt
			lua_pushstring(L, MetaKeySet);	// : tb, key, val, mt, ".set"
			lua_rawget(L, -2);				// : tb, key, val, mt, sets
			lua_pushvalue(L, 2);			// : tb, key, val, mt, sets, key
			lua_rawget(L, -2);				// : tb, key, val, mt, sets, set
			if (lua_isfunction(L, -1)) {
				lua_pushvalue(L, 3);
				lua_call(L, 1, 0);
			} else {
				lua_pop(L, 3);	// : tb, key, val,
				lua_rawset(L, 1);
			}
			return 0;
		}

		static int ReadOnly(lua_State *L) {
			return luaL_error(L, "property `%s` is read-only", lua_tostring(L, lua_upvalueindex(1)));
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 模块
	class ModuleMeta {
	public:
		ModuleMeta(LuaRef m)
			: module_(m)
		{
			state_ = module_.GetState();
			Init();
		}

		ModuleMeta(lua_State *L)
			: state_(L)
		{
			module_ = LuaRef::MakeTable(state_);
			Init();
		}

	public:
		LuaRef Ref() const {
			return module_;
		}

		// 注册函数
		template <typename F>
		ModuleMeta& Function(const char *name, const F& fn) {
			using FuncProxy = CppFuncProxy<F>;
			module_.RawSet(name, LuaRef::MakeFunction(state_, FuncProxy::Proxy, FuncProxy::Function(fn)));
			return *this;
		}

		// 注册属性
		template <typename FG, typename FS>
		ModuleMeta& Property(const char *name, const FG& get, const FS& set) {
			if constexpr (!std::is_same_v<FG, std::nullptr_t>) {
				auto gets = module_.RawGet(MetaKeyGet);
				using FGProxy = CppFuncProxy<FG>;
				gets.RawSet(name, LuaRef::MakeFunction(state_, FGProxy::Proxy, FGProxy::Function(get)));
			}

			auto sets = module_.RawGet(MetaKeySet);
			if constexpr (!std::is_same_v<FS, std::nullptr_t>) {
				using FSProxy = CppFuncProxy<FS>;
				sets.RawSet(name, LuaRef::MakeFunction(state_, FSProxy::Proxy, FSProxy::Function(set)));
			} else {
				sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
			}
			return *this;
		}

		template <typename P>
		ModuleMeta& Property(const char *name, P& prop, bool is_read_only = false) {
			auto gets = module_.RawGet(MetaKeyGet);
			gets.RawSet(name, LuaRef::MakeCClosure(state_, &CppPropertyGetter<P>::Proxy, (void*)&prop));

			auto sets = module_.RawGet(MetaKeySet);
			if constexpr (!std::is_const_v<P>) {
				if (is_read_only) {
					sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
				} else {
					sets.RawSet(name, LuaRef::MakeCClosure(state_, &CppPropertySetter<P>::Proxy, (void*)&prop));
				}
			} else {
				sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
			}

			return *this;
		}

		// 注册脚本变量
		template <typename T>
		ModuleMeta& ScriptVal(const char *name, T val) {
			module_.RawSet(name, val);
			return *this;
		}

	private:
		void Init() {
			MixMetaEvent::Init(state_);
			if (!module_.GetMetatable()) {
				module_.RawSet("__index", &ModuleMetaEvent::index);
				module_.RawSet("__newindex", &ModuleMetaEvent::newindex);
				module_.RawSet(MetaKeyGet, LuaRef::MakeTable(state_));
				module_.RawSet(MetaKeySet, LuaRef::MakeTable(state_));
				module_.SetMetatable(module_);
			}
		}

	private:
		LuaRef module_;
		lua_State* state_;
	};

	//////////////////////////////////////////////////////////////////////////
	// 类元方法
	struct ClassMetaEvent {
		static int index(lua_State* L) {
			// 优先peer表
			int peer_type = lua_getuservalue(L, 1);
			if (peer_type == LUA_TTABLE) { // :ud, key, peer
				lua_pushvalue(L, 2); // :ud, key, peer, key
				if (lua_gettable(L, -2) != LUA_TNIL) { // :ud, key, peer, val
					return 1;
				}
			}
			lua_settop(L, 2); // :ud, key
			// 元表
			lua_pushvalue(L, 1); // :ud, key, ud
			while (lua_getmetatable(L, -1)) { // :ud, key, ud, mt
				lua_remove(L, -2); // :ud, key, mt
				lua_pushvalue(L, 2); // :ud, key, mt, key
				if (lua_rawget(L, -2) != LUA_TNIL) { // :ud, key, mt, val
					return 1;
				}
				lua_pop(L, 1);	// :ud, key, mt
				// 检索属性
				lua_pushstring(L, MetaKeyGet); // :ud, key, mt, MetaKeyGet
				if (lua_rawget(L, -2) == LUA_TTABLE) { // :ud, key, mt, mt[".get"]
					lua_pushvalue(L, 2); // :ud, key, mt, mt[".get"], key
					lua_rawget(L, -2); // :ud, key, mt, mt[".get"], val
					if (lua_iscfunction(L, -1)) {
						lua_pushvalue(L, 1);
						lua_pushvalue(L, 2);
						lua_call(L, 2, 1);
						return 1;
					}
				}
				lua_settop(L, 3); // :ud, key, mt
			}
			lua_pushnil(L);
			return 1;
		}

		static int newindex(lua_State* L) {
			int self_type = lua_type(L, 1);
			lua_getmetatable(L, 1);
			while (lua_istable(L, -1)) // :ud, k, v, mt
			{
				// 检索属性
				lua_pushstring(L, MetaKeySet); // :ud, k, v, mt, MetaKeySet
				if (lua_rawget(L, -2) == LUA_TTABLE) { // :ud, k, v, mt, mt[".set"]
					lua_pushvalue(L, 2); // :ud, k, v, mt, mt[".set"], k
					lua_rawget(L, -2); // :ud, k, v, mt, mt[".set"], kv
					if (lua_iscfunction(L, -1)) {
						if (LUA_TUSERDATA == self_type) {
							lua_pushvalue(L, 1);
							lua_pushvalue(L, 3);
							lua_call(L, 2, 0);
						} else {
							lua_pushvalue(L, 3);
							lua_call(L, 1, 0);
						}
						return 0;
					}
					lua_pop(L, 1); // :ud, k, v, mt, mt[".set"]
				}
				lua_pop(L, 1); // :ud, k, v, mt
				if (!lua_getmetatable(L, -1)) {
					lua_pushnil(L);
				}
				lua_remove(L, -2); // :ud, k, v, nmt
			}
			lua_settop(L, 3); // :ud, k, v
			// 保存到peer表中
			if (LUA_TUSERDATA == self_type) {
				if (lua_getuservalue(L, 1) == LUA_TNIL) {
					lua_pop(L, 1);
					lua_newtable(L); // :ud, k, v, peer
					lua_pushvalue(L, -1); // :ud, k, v, peer, peer
					lua_setuservalue(L, 1); // :ud, k, v, peer
				}
				if (lua_istable(L, -1)) { // :ud, k, v, peer
					lua_insert(L, -3); // :ud, peer, k, v
					lua_settable(L, -3); // :ud, peer
				}
			} else {
				lua_rawset(L, 1);
			}
			lua_settop(L, 1);
			return 0;
		}

		template <typename C>
		static int gc(lua_State *L) {
			// 注意，这里先判断是否是ud，由于存在元表继承的情况，子类的元表由于设置了父类的元表，而导致其也会走finalizer流程（lua5.3开始）
			if (LUA_TUSERDATA != lua_type(L, 1)) {
				return 0;
			}
			// 查询全局gc表，以确定是否需要析构
			void* ud = *((void**)lua_touserdata(L, 1));
			lua_pushstring(L, LUAMIX_KEY_COLLECT);
			lua_rawget(L, LUA_REGISTRYINDEX); // :ud, collect

			if (LUA_TNUMBER == lua_rawgetp(L, -1, ud)) {
				// 找出对应的gc函数
				luaL_getmetatable(L, ClassTypeSignature<C>::Value()); // :ud, collect, gc_id, mt
				lua_pushstring(L, MetaKeyGC); // :ud, collect, gc_id, mt, ".gc"
				lua_rawget(L, -2); // :ud, collect, gc_id, mt, .gc
				lua_rawgeti(L, -1, lua_tointeger(L, -3)); // :ud, collect, gc_id, mt, .gc, gc_fn
				lua_pushvalue(L, 1); // :ud, collect, gc_id, mt, .gc, gc_fn, ud
				// 这里不做判断，直接调用，如果出错，就直接引发异常
				lua_call(L, 1, 0); // :ud, collect, gc_id, mt, .gc
				// 最后清理标记
				lua_pushnil(L); // :ud, collect, gc_id, mt, .gc, nil
				lua_rawsetp(L, 2, ud); // :ud, collect, gc_id, mt, .gc
			}

			return 0;
		}

		template <typename C>
		static void BuildMetatable(LuaRef mt) {
			mt.RawSet(MetaKeyGet, LuaRef::MakeTable(mt.GetState()));
			mt.RawSet(MetaKeySet, LuaRef::MakeTable(mt.GetState()));
			mt.RawSet(MetaKeySuper, LuaRef::MakeTable(mt.GetState()));
			mt.RawSet("__index", &index);
			mt.RawSet("__newindex", &newindex);
			mt.RawSet("__gc", &gc<C>);
		}

		/* 如果BuildSuper后，其super又BuildSuper了，则会导致super的super不是自己的super；
		 * 反映到代码运行中，会出现脚本中已有一个孙子类对象指针，然后再将此指针以祖父类指针压入时，
		 * 对象的原表会被重新绑定为祖父类对应的原表，导致其成员方法缺失！
		 * 然而，这种情况暂时并无彻底避免的方法，唯一可以做的就是导出类时，依照继承顺序，从父类开始依次导出子孙类，这本身就是很自然的事情；
		*/
		static void BuildSuper(LuaRef cur, LuaRef super) {
			auto cur_super = cur.RawGet(MetaKeySuper);
			cur_super.RawSet(super.RawGet("__name"), true);

			auto super_super = super.RawGet(MetaKeySuper);
			for (auto it = super_super.Next(); it.first; it = super_super.Next(it.first)) {
				cur_super.RawSet(it.first, it.second);
			}
		}

		static void BuildInhertance(LuaRef child, LuaRef parent) {
			// 共享ubox
			if (auto parent_ubox = parent.RawGet(MetaKeyUBox)) {
				child.RawSet(MetaKeyUBox, parent_ubox);
			} else if (auto child_ubox = child.RawGet(MetaKeyUBox)) {
				parent.RawSet(MetaKeyUBox, child_ubox);
			} else {
				auto ubox = LuaRef::MakeTable(child.GetState());
				ubox.RawSet("__mode", "v");
				ubox.SetMetatable(ubox);
				parent.RawSet(MetaKeyUBox, ubox);
				child.RawSet(MetaKeyUBox, ubox);
			}
			// 元表继承
			child.SetMetatable(parent);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 类元信息
	template <typename C>
	class ClassMeta {
	public:
		ClassMeta() = delete;

		ClassMeta(lua_State* L, const char* class_name, const char* const_name)
			: state_(L)
		{
			MixMetaEvent::Init(state_);

			// 这里将字符串内化到lua中，并通过注册表挂住，以免引用了一个栈变量
			class_name = lua_pushstring(state_, class_name);
			luaL_ref(state_, LUA_REGISTRYINDEX);
			const_name = lua_pushstring(state_, const_name);
			luaL_ref(state_, LUA_REGISTRYINDEX);

			ClassTypeSignature<C>::Value() = class_name;
			ClassConstSignature<C>::Value() = const_name;

			if (class_mt_ = LuaRef::RefMetatable(state_, class_name)) {
				const_mt_ = LuaRef::RefMetatable(state_, const_name);
			} else {
				class_mt_ = LuaRef::MakeMetatable(state_, class_name);
				const_mt_ = LuaRef::MakeMetatable(state_, const_name);
				ClassMetaEvent::BuildMetatable<C>(class_mt_);
				ClassMetaEvent::BuildMetatable<C>(const_mt_);
				ClassMetaEvent::BuildSuper(class_mt_, const_mt_);
				ClassMetaEvent::BuildInhertance(const_mt_, class_mt_);
			}
		}

	public:
		template <typename P>
		ClassMeta<C>& Inhertance() {
			static_assert(std::is_base_of_v<P, C>);

			auto parent_class_mt = LuaRef::RefMetatable(state_, ClassTypeSignature<P>::Value());
			if (!parent_class_mt) {
				std::string errmsg = "try to inherit a undefined class type with ";
				throw std::runtime_error(errmsg + ClassTypeSignature<C>::Value());
			}

			auto parent_const_mt = LuaRef::RefMetatable(state_, ClassConstSignature<P>::Value());
			ClassMetaEvent::BuildSuper(const_mt_, parent_const_mt);
			ClassMetaEvent::BuildSuper(class_mt_, parent_class_mt);
			ClassMetaEvent::BuildInhertance(class_mt_, parent_class_mt);

			return *this;
		}

		// 注册成员方法
		template <typename M>
		ClassMeta<C>& Method(const char* name, const M& method) {
			auto wrap = std::mem_fn(method);
			using MethodProxy = CppMethodProxy<M, decltype(wrap)>;
			class_mt_.RawSet(name, LuaRef::MakeFunction(state_, MethodProxy::Proxy, wrap));
			return *this;
		}

		// 注册成员变量为属性
		template <typename V>
		ClassMeta<C>& Property(const char* name, V C::* member) {
			auto wrap = std::mem_fn(member);
			using MemeberVariableGetter = CppMemeberVariableGetter<decltype(wrap), C, V>;
			auto gets = class_mt_.RawGet(MetaKeyGet);
			gets.RawSet(name, LuaRef::MakeFunction(state_, MemeberVariableGetter::Proxy, wrap));

			using MemeberVariableSetter = CppMemeberVariableSetter<decltype(wrap), C, V>;
			auto sets = class_mt_.RawGet(MetaKeySet);
			sets.RawSet(name, LuaRef::MakeFunction(state_, MemeberVariableSetter::Proxy, wrap));
			return *this;
		}

		// 注册静态成员变量为属性
		template <typename P>
		ClassMeta<C>& StaticProperty(const char *name, P& prop, bool is_read_only = false) {
			auto gets = class_mt_.RawGet(MetaKeyGet);
			gets.RawSet(name, LuaRef::MakeCClosure(state_, &CppPropertyGetter<P>::Proxy, (void*)&prop));

			auto sets = class_mt_.RawGet(MetaKeySet);
			if constexpr (!std::is_const_v<P>) {
				if (is_read_only) {
					sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
				} else {
					sets.RawSet(name, LuaRef::MakeCClosure(state_, &CppPropertySetter<P>::Proxy, (void*)&prop));
				}
			} else {
				sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
			}

			return *this;
		}

		// 注册外部函数形式的属性
		template <typename FG, typename FS>
		ClassMeta<C>& Property(const char *name, const FG& get, const FS& set) {
			if constexpr (!std::is_same_v<FG, std::nullptr_t>) {
				using FGProxy = CppFuncProxy<FG>;
				static_assert( std::tuple_size_v< typename FGProxy::SwapList > >= 1, "class `C` 's property getter need 1 param at least.");

				using FirstArgType = typename std::tuple_element_t<0, typename FGProxy::SwapList>::ArgHoldType;
				static_assert( std::is_convertible_v<const C*, FirstArgType>, "class `C` 's property getter's first param should be `const C*` convertible to." );

				auto gets = class_mt_.RawGet(MetaKeyGet);
				gets.RawSet(name, LuaRef::MakeFunction(state_, FGProxy::Proxy, FGProxy::Function(get)));
			}

			auto sets = class_mt_.RawGet(MetaKeySet);
			if constexpr (!std::is_same_v<FS, std::nullptr_t>) {
				using FSProxy = CppFuncProxy<FS>;
				static_assert(std::tuple_size_v< typename FSProxy::SwapList > >= 2, "class `C` 's property getter need 2 param at least.");

				using FirstArgType = typename std::tuple_element_t<0, typename FSProxy::SwapList>::ArgHoldType;
				static_assert(std::is_convertible_v<C*, FirstArgType>, "class `C` 's property setter's first param should be `C*` convertible to.");

				sets.RawSet(name, LuaRef::MakeFunction(state_, FSProxy::Proxy, FSProxy::Function(set)));
			} else {
				sets.RawSet(name, LuaRef::MakeCClosure(state_, &ModuleMetaEvent::ReadOnly, name));
			}

			return *this;
		}

		// 注册函数；主要用于静态成员函数，亦可用于类外部扩充成员函数
		template <typename F>
		ClassMeta<C>& Function(const char *name, const F& fn) {
			using FuncProxy = CppFuncProxy<F>;
			class_mt_.RawSet(name, LuaRef::MakeFunction(state_, FuncProxy::Proxy, FuncProxy::Function(fn)));
			return *this;
		}

		// 注册lua_CFunction闭包
		template <typename... Ts>
		ClassMeta<C>& CClosure(const char* name, lua_CFunction fn, Ts&&... ups) {
			class_mt_.RawSet(name, LuaRef::MakeCClosure(state_, fn, std::forward<Ts>(ups)...));
			return *this;
		}

		// 注册工厂及回收函数
		template <typename FF, typename FC>
		ClassMeta<C>& Factory(const char* name, const FF& factory, const FC& collect) {
			if (!class_mt_.RawGet(MetaKeyGC)) {
				class_mt_.RawSet(MetaKeyGC, LuaRef::MakeTable(state_));
			}

			// 先注册回收函数
			using GCProxy = CppFuncProxy<FC>;
			auto gcs = class_mt_.RawGet(MetaKeyGC);
			gcs.RawSet(gcs.Len() + 1, LuaRef::MakeFunction(state_, GCProxy::Proxy, GCProxy::Function(collect)));

			// 注册工厂函数
			using FactoryProxy = CppFactoryProxy<FF>;
			class_mt_.RawSet(name, LuaRef::MakeFactory(state_, FactoryProxy::Factory, FactoryProxy::Function(factory), gcs.Len()));

			return *this;
		}

		// 注册默认工厂及回收函数
		ClassMeta<C>& DefaultFactory() {
			if (!class_mt_.RawGet(MetaKeyGC)) {
				class_mt_.RawSet(MetaKeyGC, LuaRef::MakeTable(state_));
			}

			using GCProxy = CppFuncProxy<decltype(&defaultGC)>;
			auto gcs = class_mt_.RawGet(MetaKeyGC);
			gcs.RawSet(gcs.Len() + 1, LuaRef::MakeFunction(state_, GCProxy::Proxy, GCProxy::Function(&defaultGC)));

			using FactoryProxy = CppFactoryProxy<decltype(&defaultFactory)>;
			class_mt_.RawSet("__call", LuaRef::MakeFactory(state_, FactoryProxy::Factory, FactoryProxy::Function(&defaultFactory), gcs.Len()));

			return *this;
		}

		// 注册脚本变量
		template <typename T>
		ClassMeta<C>& ScriptVal(const char* name, T val) {
			class_mt_.RawSet(name, val);
			return *this;
		}

		LuaRef RefClassMetatable() const {
			return class_mt_;
		}

	private:
		static C * defaultFactory() {
			return new C;
		}

		static void defaultGC(const C * p) {
			delete p;
		}

	private:
		lua_State* state_;
		LuaRef class_mt_;
		LuaRef const_mt_;
	};
}