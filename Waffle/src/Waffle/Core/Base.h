#pragma once

#include "Waffle/Core/PlatformDetection.h"

#include <memory>

#ifdef WF_DEBUG
	#define WF_ENABLE_ASSERTS
#endif

#define BIT(x) (1 << x)

#define WF_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Waffle {

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	using Ref = std::shared_ptr<T>;
}