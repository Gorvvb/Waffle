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

#include "Waffle/Core/Log.h"

#ifdef WF_PLATFORM_WINDOWS
#include <Windows.h>
#endif