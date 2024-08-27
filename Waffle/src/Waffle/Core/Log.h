#pragma once

#include "wfpch.h"
#include "Waffle/Core/Base.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Waffle {

	class WAFFLE_API Log
	{
	public:
		static void Init();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define WF_CORE_TRACE(...)      ::Waffle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WF_CORE_INFO(...)       ::Waffle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WF_CORE_WARN(...)       ::Waffle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WF_CORE_ERROR(...)      ::Waffle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define WF_CORE_FATAL(...)      ::Waffle::Log::GetCoreLogger()->fatal(__VA_ARGS__)

// Client log macros
#define WF_TRACE(...)      ::Waffle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WF_INFO(...)       ::Waffle::Log::GetClientLogger()->info(__VA_ARGS__)
#define WF_WARN(...)       ::Waffle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WF_ERROR(...)      ::Waffle::Log::GetClientLogger()->error(__VA_ARGS__)
#define WF_FATAL(...)      ::Waffle::Log::GetClientLogger()->fatal(__VA_ARGS__)