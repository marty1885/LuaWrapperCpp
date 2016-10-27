#ifndef LUA_CALLBACKS_H
#define LUA_CALLBACKS_H


#include <typeinfo>
#include <typeindex>
#include <functional>
#include <algorithm>
#include <tuple>
#include <unordered_map>


#include "../Strings/MyConstString.h"

#include "./LuaScript.h"

//=============================================================================================
// Helper templates
//=============================================================================================


template<class _Ty>
struct my_decay
{
	// determines decayed version of _Ty
	typedef typename std::decay<_Ty>::type _Ty1;

	typedef typename std::_If<
		std::is_arithmetic<_Ty1>::value, typename _Ty1,
		typename std::_If<
		std::is_same<MyStringAnsi, _Ty1>::value, typename _Ty1,
		typename _Ty
		>::type
	>::type type;
};


//=============================================================================================
// Tuple manipulation
//=============================================================================================

template <typename ...Args>
struct getTuple;

template <typename T, typename ...Args>
struct getTuple<T, Args...>
{
	static inline std::tuple<T, Args...> get(MyUtils::LuaScript * script)
	{
		/*
		std::tuple<T> t = std::make_tuple<T>(script->GetFnInput<T>());
		std::tuple<Args...> args = getTuple<Args...>::get(script);
		return std::tuple_cat(t, args);
		*/

		std::tuple<T> t = std::forward_as_tuple(script->GetFnInput<T>());
		std::tuple<Args...> args = getTuple<Args...>::get(script);
		return std::tuple_cat(std::move(t), std::move(args));

		/*
		//reversed order
		std::tuple<Args...> args = getTuple<Args...>::get(script);
		std::tuple<T> t = std::make_tuple<T>( script->GetFnInput<T>());
		return std::tuple_cat(t, args);
		*/
	}
};

template <>
struct getTuple<>
{
	static inline std::tuple<> get(MyUtils::LuaScript * script)
	{
		return std::make_tuple<>();
	}
};


//=================================================================================================
// Helper templates for class
//=================================================================================================


template <class MT>
struct count_class_method_args;

template <class ClassType, class Res, class... Args>
struct count_class_method_args<Res(ClassType::*)(Args...)>
{
	static constexpr std::size_t value = sizeof...(Args);
};


//=======================================================================================

template <class MT>
struct class_method_info;

template <class T, class Res, class... Args>
struct class_method_info<Res(T::*)(Args...)>
{
	typedef std::tuple<Args&&...> ArgsTuple;
	typedef T ClassType;
	typedef Res RetVal;
	static constexpr std::size_t ArgsCount = sizeof...(Args);
	static constexpr bool IsClassMethod = true;	
	//typedef count_method_args<T>::value ArgsCount;
};

//=======================================================================================





template <class MT, MT, class I = std::make_index_sequence<count_class_method_args<MT>::value>>
struct func_impl_class_method;

template <class ClassType,
	class Res,
	class... Args,
	Res(ClassType::*MethodName)(Args...),
	std::size_t... Is
>
struct func_impl_class_method<Res(ClassType::*)(Args...), MethodName, std::index_sequence<Is...>>
{
	/*
	void operator()(ClassType * obj, std::tuple<Args...> &&tup)
	{
	(obj->*MethodName)(std::forward<Args>((std::get<Is>(tup)))...);
	}
	*/

	static void call(ClassType * obj, MyUtils::LuaScript * s)
	{
		//auto tmp = getTuple<typename std::decay<Args>::type...>::get(s);
		//auto tmp = getTuple<Args&&...>::get(s);

		auto tmp = getTuple<typename my_decay<Args>::type...>::get(s);
		//auto tmp = getTuple2<typename std::decay<Args&&>::type...>::get(s);
		//auto tmp = getTuple2<typename std::decay<Args>::type&&...>::get(s);
		//auto tmp = getTuple2<Args...>::get(s);
		
		//tuple_extractor<sizeof...(Args)>::extractTuple<ClassType, Res(ClassType::*)(Args...), Args...>(obj, MethodName, tmp);
		
		(obj->*MethodName)(std::forward<Args>(std::get<Is>(tmp))...);		
	}

	static Res callWithReturn(ClassType * obj, MyUtils::LuaScript * s)
	{
		auto tmp = getTuple<typename my_decay<Args>::type...>::get(s);

		//tuple_extractor<sizeof...(Args)>::extractTuple<ClassType, Res(ClassType::*)(Args...), Args...>(obj, MethodName, tmp);

		return (obj->*MethodName)(std::forward<Args>(std::get<Is>(tmp))...);
	}
};


//=================================================================================================
// Helper templates for method
//=================================================================================================


template <class MT>
struct count_method_args;

template <class Res, class... Args>
struct count_method_args<Res(*)(Args...)>
{
	static constexpr std::size_t value = sizeof...(Args);
};

//=======================================================================================

template <class MT>
struct method_info;

template <class Res, class... Args>
struct method_info<Res(*)(Args...)>
{
	typedef std::tuple<Args&&...> ArgsTuple;
	typedef Res RetVal;
	static constexpr std::size_t ArgsCount = sizeof...(Args);
	static constexpr bool IsClassMethod = false;	
	//typedef count_method_args<T>::value ArgsCount;
};

//=======================================================================================

template <class MT, MT, class I = std::make_index_sequence<count_method_args<MT>::value>>
struct func_impl_method;

template <class Res,
	class... Args,
	Res(*MethodName)(Args...),
	std::size_t... Is
>
struct func_impl_method<Res(*)(Args...), MethodName, std::index_sequence<Is...>>
{
	
	static void call(MyUtils::LuaScript * s)
	{
		auto tmp = getTuple<typename my_decay<Args>::type...>::get(s);
		(MethodName)(std::forward<Args>((std::get<Is>(tmp)))...);
	}

	static Res callWithReturn(MyUtils::LuaScript * s)
	{
		auto tmp = getTuple<typename my_decay<Args>::type...>::get(s);
		return (MethodName)(std::forward<Args>((std::get<Is>(tmp)))...);
	}
};





//=============================================================================================
// Main callback struct
//=============================================================================================


//=============================================================================================
// Some helper DEFINEs

#define CLASS_METHOD(ClassName, MethodName) \
	LuaCallbacks::function<decltype(&ClassName::MethodName), &ClassName::MethodName>

#define METHOD(MethodName) \
	LuaCallbacks::function<decltype(&MethodName), &MethodName>


//=============================================================================================
//main part of the callbacks


struct LuaCallbacks
{	
	//http://stackoverflow.com/questions/29194858/order-of-function-calls-in-variadic-template-expansion

	//=============================================================================================
	// Class callbacks
	//=============================================================================================

#define CLASS_FUNCTION_BODY \
		MethodInfo::ClassType *a = LuaCallbacks::GetPtr<MethodInfo::ClassType>(L, 1); \
		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L); \
		script->Reset(); \
		script->IncStack(); 


	template <class MethodType, MethodType MethodName,
		typename MethodInfo = class_method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount > 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == true)>::type* = nullptr
	>
	static int function(lua_State *L)
	{
		CLASS_FUNCTION_BODY;

		func_impl_class_method<MethodType, MethodName>::call(a, script);

		return script->GetFnReturnValueCount();
	}


	template <class MethodType, MethodType MethodName,
		typename MethodInfo = class_method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <!std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount > 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == true)>::type* = nullptr
	>
	static int function(lua_State *L)
	{
		CLASS_FUNCTION_BODY;

		script->AddFnReturnValue(func_impl_class_method<MethodType, MethodName>::callWithReturn(a, script));

		return script->GetFnReturnValueCount();
	}


	template <class MethodType, MethodType MethodName,
		typename MethodInfo = class_method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount == 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == true)>::type* = nullptr
	>
	static int function(lua_State *L)
	{
		CLASS_FUNCTION_BODY;

		(a->*MethodName)();

		return script->GetFnReturnValueCount();
	}

	template <class MethodType, MethodType MethodName,
		typename MethodInfo = class_method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <!std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount == 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == true)>::type* = nullptr
	>
	static int function(lua_State *L)
	{
		CLASS_FUNCTION_BODY;

		script->AddFnReturnValue((a->*MethodName)());

		return script->GetFnReturnValueCount();
	}

	//=============================================================================================
	// Method callbacks
	//=============================================================================================

	

	template <class MethodType, MethodType MethodName,
		typename MethodInfo = method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount > 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == false)>::type* = nullptr
	>
	static int function(lua_State *L)
	{
		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L);
		script->Reset(); 
		
		printf("Function call");

		func_impl_method<MethodType, MethodName>::call(script);

		return script->GetFnReturnValueCount();
	}

	template <class MethodType, MethodType MethodName,
		typename MethodInfo = method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <!std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount > 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == false)>::type* = nullptr
	>
	static int function(lua_State *L)
	{

		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L);
		script->Reset();
		
		printf("Function call");

		script->AddFnReturnValue(func_impl_method<MethodType, MethodName>::callWithReturn(script));


		return script->GetFnReturnValueCount();
	}

	template <class MethodType, MethodType MethodName,
		typename MethodInfo = method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,
		typename std::enable_if <std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount == 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == false)>::type* = nullptr
	>
	static int function(lua_State *L)
	{

		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L);
		script->Reset();
				
		(MethodName)();

		return script->GetFnReturnValueCount();
	}

	template <class MethodType, MethodType MethodName,
		typename MethodInfo = method_info<MethodType>,
		typename RetVal = MethodInfo::RetVal,			
		typename std::enable_if <!std::is_same<void, RetVal>::value>::type* = nullptr,
		typename std::enable_if <(MethodInfo::ArgsCount == 0), void>::type* = nullptr,
		typename std::enable_if <(MethodInfo::IsClassMethod == false)>::type* = nullptr
	>
	static int function(lua_State *L)
	{

		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L);
		script->Reset();
				
		script->AddFnReturnValue((MethodName)());

		return script->GetFnReturnValueCount();
	}


	
//protected:

	static std::unordered_map<MyConstString, std::function<void*(MyUtils::LuaScript *)> > ctors;

	template <typename T>
	static T * GetPtr(lua_State *L, int narg)
	{

		int argType = lua_type(L, narg);

		if (argType == LUA_TUSERDATA)
		{
			void * ud = NULL;
#ifdef _DEBUG		
			ud = luaL_checkudata(L, narg, lua_tostring(L, lua_upvalueindex(1)));
#else
			ud = lua_touserdata(L, narg); //"unsafe" - can run in "release", 
										  // because we check bugs in debug
#endif
			if (!ud)
			{
				//luaL_typerror(L, narg, className);
				MY_LOG_ERROR("Type ad the stack top is not LUA_TUSERDATA");
				return NULL;
			}
			//return   (Account *)ud; 
			return   *(T **)ud;  // unbox pointer
		}
		else if (argType == LUA_TLIGHTUSERDATA)
		{
			void * data = lua_touserdata(L, narg);
			return static_cast<T *>(data);
		}
		else
		{
			MY_LOG_ERROR("Type ad the stack top is not LUA_TUSERDATA OR LUA_TLIGHTUSERDATA");
			return NULL;
		}
	}

	template <typename T>
	static int create_new(lua_State *L)
	{

		T ** udata = (T **)lua_newuserdata(L, sizeof(T *));

		//double balance = luaL_checknumber(L, 1);		
		//*udata = new T(balance);

		std::function<void*(MyUtils::LuaScript *)> f = LuaCallbacks::ctors[MyConstString(typeid(T).name())];

		MyUtils::LuaScript * script = MyUtils::LuaWrapper::GetInstance()->GetScript(L);
		script->Reset();
		T * newInstance = static_cast<T *>(f(script));
		*udata = newInstance;

		luaL_getmetatable(L, lua_tostring(L, lua_upvalueindex(1)));
		lua_setmetatable(L, -2);
		return 1;

	}

	template <typename T>
	static int garbage_collect(lua_State *L)
	{
		printf("***GC\n");

		int argType = lua_type(L, 1);
		if (argType == LUA_TUSERDATA)
		{			
			T* a = (*(T **)(lua_touserdata(L, 1)));
			delete a;
		}
		return 0;
	}


};

std::unordered_map<MyConstString, std::function<void*(MyUtils::LuaScript *)> > LuaCallbacks::ctors;


#endif