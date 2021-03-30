#pragma once

#include <stdexcept>

#include "lua_ref.h"

namespace LuaMix::Impl {
	//////////////////////////////////////////////////////////////////////////
	// ȫ�ַ���
	struct MixMetaEvent {
		static void Init(lua_State* L) {
			StackGuard _guard(L);

			lua_pushstring(L, LUAMIX_KEY_INIT);		// :key
			lua_rawget(L, LUA_REGISTRYINDEX);		// :flag
			if (!lua_isboolean(L, -1)) {
				lua_pushstring(L, LUAMIX_KEY_INIT);	// :nil, key
				lua_pushboolean(L, 1);				// :nil, key, flag
				lua_rawset(L, LUA_REGISTRYINDEX);	// :nil

				// �ӹ��������ڵ�ud����
				lua_pushstring(L, LUAMIX_KEY_COLLECT);	// :nil, key
				lua_newtable(L);						// :nil, key, tb
				lua_rawset(L, LUA_REGISTRYINDEX);		// :nil

				// ���ߺ���
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
	// ģ��Ԫ����
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
	// ģ��
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

		// ע�ắ��
		template <typename F>
		ModuleMeta& Function(const char *name, const F& fn) {
			using FuncProxy = CppFuncProxy<F>;
			module_.RawSet(name, LuaRef::MakeFunction(state_, FuncProxy::Proxy, FuncProxy::Function(fn)));
			return *this;
		}

		// ע������
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

		// ע��ű�����
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
	// ��Ԫ����
	struct ClassMetaEvent {
		static int index(lua_State* L) {
			// ����peer��
			int peer_type = lua_getuservalue(L, 1);
			if (peer_type == LUA_TTABLE) { // :ud, key, peer
				lua_pushvalue(L, 2); // :ud, key, peer, key
				if (lua_gettable(L, -2) != LUA_TNIL) { // :ud, key, peer, val
					return 1;
				}
			}
			lua_settop(L, 2); // :ud, key
			// Ԫ��
			lua_pushvalue(L, 1); // :ud, key, ud
			while (lua_getmetatable(L, -1)) { // :ud, key, ud, mt
				lua_remove(L, -2); // :ud, key, mt
				lua_pushvalue(L, 2); // :ud, key, mt, key
				if (lua_rawget(L, -2) != LUA_TNIL) { // :ud, key, mt, val
					return 1;
				}
				lua_pop(L, 1);	// :ud, key, mt
				// ��������
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
			lua_getmetatable(L, 1);
			while (lua_istable(L, -1)) // :ud, k, v, mt
			{
				// ��������
				lua_pushstring(L, MetaKeySet); // :ud, k, v, mt, MetaKeySet
				if (lua_rawget(L, -2) == LUA_TTABLE) { // :ud, k, v, mt, mt[".set"]
					lua_pushvalue(L, 2); // :ud, k, v, mt, mt[".set"], k
					lua_rawget(L, -2); // :ud, k, v, mt, mt[".set"], kv
					if (lua_iscfunction(L, -1)) {
						lua_pushvalue(L, 1);
						lua_pushvalue(L, 3);
						lua_call(L, 2, 0);
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
			// ���浽peer����
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
			lua_settop(L, 1);
			return 0;
		}

		template <typename C>
		static int gc(lua_State *L) {
			// ע�⣬�������ж��Ƿ���ud�����ڴ���Ԫ��̳е�����������Ԫ�����������˸����Ԫ����������Ҳ����finalizer���̣�lua5.3��ʼ��
			if (LUA_TUSERDATA != lua_type(L, 1)) {
				return 0;
			}
			// ��ѯȫ��gc����ȷ���Ƿ���Ҫ����
			void* ud = *((void**)lua_touserdata(L, 1));
			lua_pushstring(L, LUAMIX_KEY_COLLECT);
			lua_rawget(L, LUA_REGISTRYINDEX); // :ud, collect

			if (LUA_TNIL != lua_rawgetp(L, -1, ud)) { // :ud, collect, flag
				// ��Ҫ����������Ԫ�����ҳ����������������ã�ע�����������׼ȷ������
				luaL_getmetatable(L, ClassTypeSignature<C>::Value()); // :ud, collect, flag, mt
				lua_pushstring(L, MetaKeyGC); // :ud, collect, flag, mt, ".gc"
				lua_rawget(L, -2); // :ud, collect, flag, mt, gc_fn
				lua_pushvalue(L, 1); // :ud, collect, flag, mt, gc_fn, ud
				lua_call(L, 1, 0); // :ud, collect, flag, mt
				// ������
				lua_pushnil(L); // :ud, collect, flag, mt, nil
				lua_rawsetp(L, 2, ud); // :ud, collect, flag, mt
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

		// ����ò���е����⣬��super��Super����cur��©�������濴�Ƿ���Ҫ�޸�
		static void BuidSuper(LuaRef cur, LuaRef super) {
			auto cur_super = cur.RawGet(MetaKeySuper);
			cur_super.RawSet(super.RawGet("__name"), true);

			auto super_super = super.RawGet(MetaKeySuper);
			for (auto it = super_super.Next(); it.first; it = super_super.Next(it.first)) {
				cur_super.RawSet(it.first, it.second);
			}
		}

		static void BuildInhertance(LuaRef child, LuaRef parent) {
			// ����ubox
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
			// Ԫ��̳�
			child.SetMetatable(parent);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// ��Ԫ��Ϣ
	template <typename C>
	class ClassMeta {
	private:
		lua_State* state_;
		LuaRef class_mt_;
		LuaRef const_mt_;

	public:
		ClassMeta() = delete;

		ClassMeta(lua_State* L, const char* class_name, const char* const_name)
			: state_(L)
		{
			MixMetaEvent::Init(state_);

			// ���ｫ�ַ����ڻ���lua�У���ͨ��ע����ס�������ⲿ����һ��ջ�����������쳣
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
				ClassMetaEvent::BuidSuper(class_mt_, const_mt_);
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
			ClassMetaEvent::BuidSuper(const_mt_, parent_const_mt);
			ClassMetaEvent::BuidSuper(class_mt_, parent_class_mt);
			ClassMetaEvent::BuildInhertance(class_mt_, parent_class_mt);

			return *this;
		}

		// ע���Ա����
		template <typename M>
		ClassMeta<C>& Method(const char* name, const M& method) {
			auto wrap = std::mem_fn(method);
			using MethodProxy = CppMethodProxy<M, decltype(wrap)>;
			class_mt_.RawSet(name, LuaRef::MakeFunction(state_, MethodProxy::Proxy, wrap));
			return *this;
		}

		// ע���Ա����Ϊ����
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

		// ע�ắ������Ҫ���ھ�̬��Ա����������������ⲿ�����Ա����
		template <typename F>
		ClassMeta<C>& Function(const char *name, const F& fn) {
			using FuncProxy = CppFuncProxy<F>;
			class_mt_.RawSet(name, LuaRef::MakeFunction(state_, FuncProxy::Proxy, FuncProxy::Function(fn)));
			return *this;
		}

		// ע��lua_CFunction�հ�
		template <typename... Ts>
		ClassMeta<C>& CClosure(const char* name, lua_CFunction fn, Ts&&... ups) {
			class_mt_.RawSet(name, LuaRef::MakeCClosure(state_, fn, std::forward<Ts>(ups)...));
			return *this;
		}

		// ע�Ṥ�������պ���
		template <typename FF, typename FC>
		ClassMeta<C>& Factory(const char* name, const FF& factory, const FC& collect) {
			// ��ע����պ���
			if constexpr (!std::is_same_v < FC, std::nullptr_t>) {
				using FuncProxy = CppFuncProxy<FC>;
				class_mt_.RawSet(MetaKeyGC, LuaRef::MakeFunction(state_, FuncProxy::Proxy, FuncProxy::Function(collect)));
			}

			// ���û��ע������պ�����������ע�Ṥ��������ֱ�����쳣
			if (!class_mt_.RawGet(MetaKeyGC)) {
				std::string err = "can't register factory to `";
				err += ClassTypeSignature<C>::Value();
				err += "`, which has no collector.";
				throw std::runtime_error(err);
			}

			// ע�Ṥ������
			using FactoryProxy = CppFactoryProxy<FF>;
			class_mt_.RawSet(name, LuaRef::MakeFunction(state_, FactoryProxy::Factory, FactoryProxy::Function(factory)));

			return *this;
		}

		// ע��ű�����
		template <typename T>
		ClassMeta<C>& ScriptVal(const char* name, T val) {
			class_mt_.RawSet(name, val);
			return *this;
		}

		LuaRef RefClassMetatable() const {
			return class_mt_;
		}
	};
}