#pragma once

#include "wfpch.h"
#include "Waffle/Core/Base.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <memory>

namespace Waffle {

	class Log
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

template<typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector)
{
	return os << glm::to_string(vector);
}

template<typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix)
{
	return os << glm::to_string(matrix);
}

template<typename OStream, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, glm::qua<T, Q> quaternion)
{
	return os << glm::to_string(quaternion);
}

// Core log macros
#define WF_CORE_TRACE(...)		::Waffle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define WF_CORE_INFO(...)		::Waffle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define WF_CORE_WARN(...)		::Waffle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define WF_CORE_ERROR(...)		::Waffle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define WF_CORE_CRITICAL(...)	::Waffle::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define WF_TRACE(...)		::Waffle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define WF_INFO(...)		::Waffle::Log::GetClientLogger()->info(__VA_ARGS__)
#define WF_WARN(...)		::Waffle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define WF_ERROR(...)		::Waffle::Log::GetClientLogger()->error(__VA_ARGS__)
#define WF_CRITICAL(...)	::Waffle::Log::GetClientLogger()->critical(__VA_ARGS__)