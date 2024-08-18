#pragma once

#ifdef WF_PLATFORM_WINDOWS
	#ifdef WF_BUILD_DLL
		#define WAFFLE_API _declspec(dllexport)
	#else
		#define WAFFLE_API _declspec(dllimport)
	#endif
#else
	#error Waffle only supports windows!
#endif

#ifdef HZ_ENABLE_ASSERTS
	#define WF_ASSERT(x, ...) { if(!(x)) { HZ_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define WF_CORE_ASSERT(x, ...) { if(!(x)) { HZ_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); }}
#else
	#define WF_ASSERT(x, ...)
	#define WF_CORE_ASSERT(x, ...)
#endif


#define BIT(x) (1 << x)

#define WF_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)