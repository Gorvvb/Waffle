#pragma once

#include "Waffle/Core/PlatformDetection.h"

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Log.h"
// Profiling is permanently off (the tracer was removed) - keep the macros as no-ops:
#define WF_PROFILE_BEGIN_SESSION(name, filepath)
#define WF_PROFILE_END_SESSION()
#define WF_PROFILE_SCOPE(name)
#define WF_PROFILE_FUNCTION()

#ifdef WF_PLATFORM_WINDOWS
#include <Windows.h>
#endif