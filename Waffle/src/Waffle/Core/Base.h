#pragma once

#include "Waffle/Core/PlatformDetection.h"

//#include "Waffle/Core/Log.h"
#include "Waffle/Core/Assert.h"

#include <memory>

#ifdef WF_DEBUG
	#if defined(WF_PLATFORM_WINDOWS)
		#define WF_DEBUGBREAK() __debugbreak()
	#elif defined(WF_PLATFORM_LINUX)
		#error "Linux isn't supported yet!'"
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define WF_ENABLE_ASSERTS
#else
	#define WF_DEBUGBREAK()
#endif

#define WF_EXPAND_MACRO(x) x
#define WF_STRINGIFY_MACRO(x) #x

#define BIT(x) (1 << x)

#define WF_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Waffle {

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}