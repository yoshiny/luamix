#pragma once

#include <tuple>
#include <functional>

#include "type_mix.h"

namespace LuaMix::Impl {
	//////////////////////////////////////////////////////////////////////////
	// 函数参数包装
	/*
	* 形参只支持：基本类型及其左值引用、类类型指针及其指针左值引用
	* T、C*：从栈中拿到值，holder为值语义，为非出参
	* T&、C*& ：从栈中拿到值，holder为引用语义，为出参
	*/
	template <typename T>
	struct CppArg {
		// 总是以值语义持有实参，无论是基本类型还是类类型指针
		using ArgHoldType = typename TypeMix<T>::ArgHoldType;

		ArgHoldType hold_;

		void Input(lua_State* L, int index) {
			hold_ = Fetch<T, true>(L, index);
		}

		int Output(lua_State* L) {
			constexpr bool need_out = std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>;
			if constexpr (need_out) {
				Push<T>(L, hold_);
				return 1;
			}
			return 0;
		}

		// 返回左值引用，以便于被代理的函数实现出参
		std::add_lvalue_reference_t<ArgHoldType> Value() {
			return hold_;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 函数代理
	template <typename F, typename R, typename... P>
	struct CppFuncProxyImpl {
		using SwapList = std::tuple<CppArg<P>...>;
		using FuncType = std::add_const_t<std::conditional_t<std::is_function_v<std::remove_pointer_t<F>>, F, std::add_pointer_t<F>>>;

		static int Proxy(lua_State* L) {
			try {
				SwapList swap_list;
				return std::apply([L](auto &... args) {
					int index = 0;
					(args.Input(L, ++index), ...);
					FuncType func = static_cast<FuncType>(lua_touserdata(L, lua_upvalueindex(1)));;
					if constexpr (std::is_same_v<R, void>) {
						std::invoke(*func, args.Value()...);
					} else {
						Push<R>(L, std::invoke(*func, args.Value()...));
					}
					return ((std::is_same_v<R, void> ? 0 : 1) + ... + args.Output(L));
					}, swap_list);
			} catch (const std::exception &e) {
				return luaL_error(L, "%s", e.what());
			}
		}

		template <typename T>
		static F Function(const T& func) {
			return static_cast<F>(func);
		}
	};

	template <typename F, typename ENABLE = void>
	struct CppFuncProxy;

	// 常规函数指针
	template<typename R, typename... P>
	struct CppFuncProxy<R(*)(P...)> : CppFuncProxyImpl<R(*)(P...), R, P...> {};

	// 常规函数对象，退化为指针处理
	template<typename F>
	struct CppFuncProxy<F, std::enable_if_t<std::is_function_v<F>>> : CppFuncProxy<std::decay_t<F>> {};

	// std::function
	template <typename R, typename... P>
	struct CppFuncProxy<std::function<R(P...)>> : CppFuncProxyImpl<std::function<R(P...)>, R, P...> {};

	// lambda 和 可调用对象
	template <typename C>
	struct CppCouldBeLambda {
		static constexpr bool value = std::is_class_v<C>;
	};

	template <typename R, typename... P>
	struct CppCouldBeLambda <std::function<R(P...)>> {
		static constexpr bool value = false;
	};

	template <typename C>
	struct CppLambdaTraits : public CppLambdaTraits <decltype(&C::operator())> {};

	template <typename C, typename R, typename... P>
	struct CppLambdaTraits <R(C::*)(P...) const> {
		using FunctionType = std::function<R(P...)>;
	};

	template <typename C, typename R, typename... P>
	struct CppLambdaTraits <R(C::*)(P...)> {
		using FunctionType = std::function<R(P...)>;
	};

	template <typename F>
	struct CppFuncProxy<F, std::enable_if_t<CppCouldBeLambda<F>::value>> : CppFuncProxy<typename CppLambdaTraits<F>::FunctionType> {};

	template <typename F>
	struct CppFactoryProxy : CppFuncProxy<F, std::enable_if_t<CppCouldBeLambda<F>::value>> {
		static int Factory(lua_State* L) {
			int ret = CppFuncProxy<F, std::enable_if_t<CppCouldBeLambda<F>::value>>::Proxy(L);
			assert(ret >= 1);
			StackGuard _guard(L);
			luaL_checktype(L, -ret, LUA_TUSERDATA);
			void* udp = *((void**)lua_touserdata(L, -ret));
			lua_pushstring(L, LUAMIX_KEY_COLLECT);
			lua_rawget(L, LUA_REGISTRYINDEX);
			lua_pushlightuserdata(L, udp);
			lua_rawsetp(L, -2, udp);
			return ret;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// 成员方法代理
	template <bool IS_CONST, typename F, typename C, typename R, typename... P>
	struct CppMethodProxyImpl {
		using SwapList = std::tuple<CppArg<std::conditional_t<IS_CONST, const C*, C*>>, CppArg<P>...>;

		static int Proxy(lua_State* L) {
			try {
				SwapList swap_list;
				auto& arg_obj = std::get<0>(swap_list);
				arg_obj.Input(L, 1);
				if (!arg_obj.Value()) {
					return luaL_error(L, "can't invoke member function with a nil value.");
				}
				return std::apply([L](auto& obj, auto &... args) {
					int index = 1;
					(args.Input(L, ++index), ...);
					auto& mem_fn = *static_cast<const F*>(lua_touserdata(L, lua_upvalueindex(1)));
					if constexpr (std::is_same_v<R, void>) {
						std::invoke(mem_fn, obj.Value(), args.Value()...);
					} else {
						Push<R>(L, std::invoke(mem_fn, obj.Value(), args.Value()...));
					}
					return ((std::is_same_v<R, void> ? 0 : 1) + ... + args.Output(L));
					}, swap_list);
			} catch (const std::exception& e) {
				return luaL_error(L, "%s", e.what());
			}
		}
	};

	template <typename M, typename W>
	struct CppMethodProxy;

	template <typename W, typename C, typename R, typename... P>
	struct CppMethodProxy<R(C::*)(P...), W> : CppMethodProxyImpl<false, W, C, R, P...> {};

	template <typename W, typename C, typename R, typename... P>
	struct CppMethodProxy<R(C::*)(P...) noexcept, W> : CppMethodProxy<R(C::*)(P...), W> {};

	template <typename W, typename C, typename R, typename... P>
	struct CppMethodProxy<R(C::*)(P...) const, W> : CppMethodProxyImpl<true, W, C, R, P...> {};

	template <typename W, typename C, typename R, typename... P>
	struct CppMethodProxy<R(C::*)(P...) const noexcept, W> : CppMethodProxy<R(C::*)(P...) const, W> {};
	//////////////////////////////////////////////////////////////////////////
	// 成员变量代理
	template <typename W, typename C, typename V>
	struct CppMemeberVariableGetter {
		static int Proxy(lua_State* L) {
			try {
				auto& mem_access = *static_cast<W*>(lua_touserdata(L, lua_upvalueindex(1)));
				const C* obj = Fetch<const C*, true>(L, 1);
				Push<V>(L, std::invoke(mem_access, obj));
				return 1;
			} catch (const std::exception& e) {
				return luaL_error(L, "%s", e.what());
			}
		}
	};

	template <typename W, typename C, typename V>
	struct CppMemeberVariableSetter {
		static int Proxy(lua_State* L) {
			try {
				auto& mem_access = *static_cast<W*>(lua_touserdata(L, lua_upvalueindex(1)));
				C* obj = Fetch<C*, true>(L, 1);
				std::invoke(mem_access, obj) = Fetch<V, true>(L, 2);
				return 0;
			} catch (const std::exception& e) {
				return luaL_error(L, "%s", e.what());
			}
		}
	};
}

