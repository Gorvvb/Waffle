#pragma once

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Log.h"
#include <filesystem>

#ifdef WF_ENABLE_ASSERTS

	#define WF_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { WF##type##ERROR(msg, __VA_ARGS__); WF_DEBUGBREAK(); } }
	#define WF_INTERNAL_ASSERT_WITH_MSG(type, check, ...) WF_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
	#define WF_INTERNAL_ASSERT_NO_MSG(type, check) WF_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", WF_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)
	
	#define WF_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define WF_INTERNAL_ASSERT_GET_MACRO(...) WF_EXPAND_MACRO( WF_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, WF_INTERNAL_ASSERT_WITH_MSG, WF_INTERNAL_ASSERT_NO_MSG) )
	
	#define WF_ASSERT(...) WF_EXPAND_MACRO( WF_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
	#define WF_CORE_ASSERT(...) WF_EXPAND_MACRO( WF_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
	#define WF_ASSERT(...)
	#define WF_CORE_ASSERT(...)
#endif