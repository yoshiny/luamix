#pragma once

#include "lualib.h"
#include "lauxlib.h"

#include <string>
#include <cassert>

#include "mix_util.h"

namespace LuaMix::Impl {
	//////////////////////////////////////////////////////////////////////////
	template <typename T, typename ENABLED = void>
	struct _type_mix_spec;

	template <typename T>
	struct _type_mix_class;

	//////////////////////////////////////////////////////////////////////////
	template <typename T, typename = std::void_t<>>
	struct _is_type_speced : std::false_type {};

	template <typename T>
	struct _is_type_speced<T, std::void_t<typename _type_mix_spec<T>::ArgHoldType>> : std::true_type {};

	template <typename T>
	inline constexpr bool _is_type_speced_v = _is_type_speced<T>::value;

	//template <typename T, bool IS_SPECED, bool IS_CLASS>
	//struct _type_mix_impl;

	//template <typename T>
	//struct _type_mix_impl<T, true, false> {
	//	using Type = _type_mix_spec<T>;
	//};

	//template <typename T>
	//struct _type_mix_impl<T, true, true> {
	//	using Type = _type_mix_spec<T>;
	//};

	//template <typename T>
	//struct _type_mix_impl<T, false, false> {
	//	using Type = _type_mix_spec<std::decay_t<T>>;
	//};

	//template <typename T>
	//struct _type_mix_impl<T, false, true> {
	//	using Type = _type_mix_class<std::decay_t<T>>;
	//};

	//template <typename T>
	//inline constexpr bool _is_class_v = std::is_class_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>>;

	//template <typename T>
	//struct TypeMix : _type_mix_impl<T, _is_type_speced_v<T>, _is_class_v<T>>::Type {};

	template <typename T>
	inline constexpr bool _is_class_v = std::is_class_v<std::remove_cv_t<std::remove_pointer_t<T>>>;

	template <typename T, typename DT = std::decay_t<T>>
	struct TypeMix : std::conditional_t< _is_type_speced_v<T>, _type_mix_spec<T>,
		std::conditional_t<_is_type_speced_v<DT>, _type_mix_spec<DT>,
		std::conditional_t<_is_class_v<DT>, _type_mix_class<DT>, _type_mix_spec<T>>
		>>
	{};

	//////////////////////////////////////////////////////////////////////////
	template <typename T>
	inline void Push(lua_State* L, const T& value) {
		TypeMix<T>::Push(L, value);
	}

	// �ṩ����char *�������Ǳ�Ҫ�ģ�`const T& value` �Ὣ�ַ�������ֵ�Ƶ�Ϊ�ַ��������ͣ����� TypeMix ʵ����ʧ�ܣ���û��Ҳ�����ж������֧��:-(
	inline void Push(lua_State* L, const char* value) {
		lua_pushstring(L, value);
	}

	inline void Push(lua_State* L, std::nullptr_t) {
		lua_pushnil(L);
	}

	template <typename T, bool CHECK = false>
	inline decltype(auto) Fetch(lua_State* L, int index) {
		return TypeMix<T>::Fetch<CHECK>(L, index);
	}

	//////////////////////////////////////////////////////////////////////////
	// ���͡�ö��ͳһ����
	template <typename T>
	struct _integer_mix_speced {
		using ArgHoldType = std::decay_t<T>;

		static void Push(lua_State* L, T value) {
			lua_pushinteger(L, static_cast<lua_Integer>(value));
		}

		template <bool CHECK = false>
		static T Fetch(lua_State* L, int index) {
			if constexpr (CHECK) {
				return static_cast<T>(luaL_checkinteger(L, index));
			} else {
				return static_cast<T>(lua_tointeger(L, index));
			}
		}
	};

	template <typename T>
	struct _type_mix_spec<T, std::enable_if_t<std::is_integral_v<T>>> : _integer_mix_speced<T> {};

	template <typename T>
	struct _type_mix_spec<T, std::enable_if_t<std::is_enum_v<T>>> : _integer_mix_speced<T> {};

	//////////////////////////////////////////////////////////////////////////
	// ������ͳһ����
	template <typename T>
	struct _type_mix_spec<T, std::enable_if_t<std::is_floating_point_v<T>>> {
		using ArgHoldType = std::decay_t<T>;

		static void Push(lua_State* L, T value) {
			lua_pushnumber(L, static_cast<lua_Number>(value));
		}

		template <bool CHECK = false>
		static T Fetch(lua_State* L, int index) {
			if constexpr (CHECK) {
				return static_cast<T>(luaL_checknumber(L, index));
			} else {
				return static_cast<T>(lua_tonumber(L, index));
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// ���������ػ�
	template <>
	struct _type_mix_spec<bool> {
		using ArgHoldType = bool;

		static void Push(lua_State* L, bool value) {
			lua_pushboolean(L, value ? 1 : 0);
		}

		template <bool CHECK = false>
		static bool Fetch(lua_State* L, int index) {
			return lua_toboolean(L, index) ? true : false;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// �ַ����ػ���ע�⣺signed char �� unsigned char ������Ϊ�ַ������������ʹ���
	template <>
	struct _type_mix_spec<char> {
		using ArgHoldType = char;

		static void Push(lua_State* L, char value) {
			char str[] = { value, 0 };
			lua_pushstring(L, str);
		}

		template <bool CHECK = false>
		static char Fetch(lua_State* L, int index) {
			if constexpr (CHECK) {
				return luaL_checkstring(L, index)[0];
			} else {
				if (auto str = lua_tostring(L, index)) {
					return str[0];
				} else {
					return '\0';
				}
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// �ػ� char *
	template <>
	struct _type_mix_spec<char*> {
		using ArgHoldType = char*;

		static void Push(lua_State* L, char* value) {
			lua_pushstring(L, value);
		}

		// lua�е��ַ������ڲ����ģ��������ﲻ�ṩ��lua�з���char *�Ĳ���
	};

	template <>
	struct _type_mix_spec<const char*> {
		using ArgHoldType = const char*;

		static void Push(lua_State* L, const char* value) {
			lua_pushstring(L, value);
		}

		template <bool CHECK = false>
		static const char* Fetch(lua_State* L, int index) {
			if constexpr (CHECK) {
				return luaL_checkstring(L, index);
			} else {
				if (auto str = lua_tostring(L, index)) {
					return str;
				} else {
					return nullptr;
				}
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// �ػ� std::string
	template <>
	struct _type_mix_spec<std::string> {
		using ArgHoldType = std::string;

		static void Push(lua_State* L, const std::string& value) {
			lua_pushlstring(L, value.data(), value.length());
		}

		template <bool CHECK = false>
		static std::string Fetch(lua_State* L, int index) {
			size_t len;
			if constexpr (CHECK) {
				const char* p = luaL_checklstring(L, index, &len);
				return std::string(p, len);
			} else {
				if (auto p = lua_tolstring(L, index, &len)) {
					return std::string(p, len);
				} else {
					return std::string();
				}
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// �ػ� std::string_view
	template <>
	struct _type_mix_spec<std::string_view> {
		using ArgHoldType = std::string_view;

		static void Push(lua_State* L, std::string_view value) {
			lua_pushlstring(L, value.data(), value.length());
		}

		template <bool CHECK = false>
		static std::string_view Fetch(lua_State* L, int index) {
			size_t len;
			if constexpr (CHECK) {
				const char* p = luaL_checklstring(L, index, &len);
				return std::string_view(p, len);
			} else {
				if (auto p = lua_tolstring(L, index, &len)) {
					return std::string_view(p, len);
				} else {
					return std::string_view();
				}
			}
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// �ػ� lua_CFunction
	template <>
	struct _type_mix_spec<lua_CFunction> {
		using ArgHoldType = lua_CFunction;

		static void Push(lua_State* L, lua_CFunction value) {
			lua_pushcfunction(L, value);
		}

		template <bool CHECK = false>
		static lua_CFunction Fetch(lua_State* L, int index) {
			return lua_tocfunction(L, index);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// ������ͳһ����
	constexpr const char* UndefinedClassType = "undefined class type";
	constexpr const char* MetaKeyGet = ".get";
	constexpr const char* MetaKeySet = ".set";
	constexpr const char* MetaKeySuper = ".super";
	constexpr const char* MetaKeyUBox = ".ubox";
	constexpr const char* MetaKeyGC = ".gc";

	template <typename C, int KIND = 0>
	struct ClassTypeSignature {
		static const char*& Value() {
			static const char* name = UndefinedClassType;
			return name;
		}
	};

	template <typename C>
	using ClassConstSignature = ClassTypeSignature<C, 1>;

	// ָ������
	template <typename C>
	struct _type_mix_class<C*> {
		using ArgHoldType = C*;

		template <typename T>
		using ClassSignature = std::conditional_t<std::is_const_v<T>, ClassConstSignature<std::decay_t<T>>, ClassTypeSignature<std::decay_t<T>>>;

		// Ч�ʿ��ǣ�ֱ�ӵ���API
		static void Push(lua_State* L, C* value) {
			if (!value) {
				lua_pushnil(L);
			} else {
				if (LUA_TTABLE != luaL_getmetatable(L, ClassSignature<C>::Value())) {
					assert(false);
					lua_pop(L, 1);
					lua_pushnil(L);
					return;
				}

				lua_pushstring(L, MetaKeyUBox);			// :mt, ".ubox"
				lua_rawget(L, -2);						// :mt, ubox
				lua_pushlightuserdata(L, (void*)value);		// :mt, ubox, lud

				if (LUA_TNIL == lua_rawget(L, -2)) {	// :mt, ubox, ubox[lud]
					// û�л��棬�½�������ubox
					lua_pop(L, 1);						// :mt, ubox
					lua_pushlightuserdata(L, (void*)value);	// :mt, ubox, lud
					*(void**)lua_newuserdata(L, sizeof(void*)) = (void*)value; // :mt, ubox, lud, ud
					lua_pushvalue(L, -1);				// :mt, ubox, lud, ud, ud
					lua_insert(L, -4);					// :mt, ud, ubox, lud, ud
					lua_rawset(L, -3);					// :mt, ud, ubox
					lua_pop(L, 1);						// :mt, ud
					lua_pushvalue(L, -2);				// :mt, ud, mt
					lua_setmetatable(L, -2);			// :mt, ud
					lua_remove(L, -2);					// :ud
				} else {
					// �л��棬����Ƿ���Ҫ���Ԫ��
					lua_insert(L, -2);					// :mt, ubox[lud], ubox
					lua_pop(L, 1);						// :mt, ud
					lua_pushstring(L, MetaKeySuper);	// :mt, ud, ".super"
					lua_rawget(L, -3);					// :mt, ud, super
					lua_pushstring(L, ClassSignature<C>::Value()); // :mt, ud, super, cur_type_str
					lua_rawget(L, -2);					// :mt, ud, super, bool
					if (1 == lua_toboolean(L, -1)) {
						// �Ѵ����������ͣ�����Ҫ���Ԫ��
						lua_pop(L, 2);					// :mt, ud
						lua_remove(L, -2);				// :ud
					} else {
						// ѹ����һ�������ε����ͣ����°�Ԫ��
						lua_pushvalue(L, -4);			// :mt, ud, super, bool, mt
						lua_setmetatable(L, -4);		// :mt, ud, super, bool
						lua_pop(L, 2);					// :mt, ud
						lua_remove(L, -2);				// :ud
					}
				}
			}
		}

		// ���Ͳ��ԵĻ���ͨ�� luaL_checkudata ���׳��쳣���Ա����ԭ���Ĵ�����Ϣ
		template <bool CHECK = false>
		static C* Fetch(lua_State* L, int index) {
			void* ud = lua_touserdata(L, index);
			switch (lua_type(L, index)) {
			case LUA_TUSERDATA:
				// �����ϸ�ƥ��
				if (lua_getmetatable(L, index)) {	// : mt
					luaL_getmetatable(L, ClassSignature<C>::Value()); // : mt, c_mt
					if (lua_rawequal(L, -1, -2)) {
						lua_pop(L, 2);
						return static_cast<C*>(*(void**)ud);
					}
				} else {
					if constexpr (CHECK) {
						return (C*)luaL_checkudata(L, index, ClassSignature<C>::Value());
					} else {
						return nullptr;
					}
				}
				// ���߸����ε�����
				lua_pop(L, 1);	// : mt
				lua_pushstring(L, MetaKeySuper);
				lua_rawget(L, -2); // : mt, super
				lua_pushstring(L, ClassSignature<C>::Value()); // : mt, super, cur_type_name
				lua_rawget(L, -2); // : mt, super, bool
				if (1 == lua_toboolean(L, -1)) {
					lua_pop(L, 3);
					return static_cast<C*>(*(void**)ud);
				} else {
					if constexpr (CHECK) {
						return (C*)luaL_checkudata(L, index, ClassSignature<C>::Value());
					} else {
						return nullptr;
					}
				}
			default:
				if constexpr (CHECK) {
					return (C*)luaL_checkudata(L, index, ClassSignature<C>::Value());
				} else {
					return nullptr;
				}
			}
		}
	};

	// ָ�������
	template <typename C>
	struct _type_mix_class<C*&> : _type_mix_class<C*> {};

}
