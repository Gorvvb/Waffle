#ifdef _WIN32
	#ifdef _WIN64
		#define WF_PLATFORM_WINDOWS
	#else
		#error "x86 Builds are not supported!"
	#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>

	#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported!"
	#elif TARGET_OS_IPHONE == 1
		#error "IOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#error "MacOS is not supported!"
	#else
		#error "Unknown Apple platform!"
	#endif
#elif defined(__ANDROID__)
	#define WF_PLATFORM_ANDROID
	#error "Android is not supported!"
#elif defined(__linux__)
	#define WF_PLATFORM_LINUX
	#error "Linux is not supported!"
#else
	#error "Unknown platform!"
#endif

#ifdef HZ_ENABLE_ASSERTS
	#define WF_ASSERT(x, ...) { if(!(x)) { HZ_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define WF_CORE_ASSERT(x, ...) { if(!(x)) { HZ_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); }}
#else
	#define WF_ASSERT(x, ...)
	#define WF_CORE_ASSERT(x, ...)
#endif

#ifdef WF_PLATFORM_WINDOWS
	#if WF_DYNAMIC_LINK
		#ifdef WF_BUILD_DLL
			#define WAFLLE_API __declspec(dllexport)
		#else
			#define WAFLLE_API __declspec(dllimport)
		#endif
	#else
		#define WAFFLE_API
	#endif
#endif