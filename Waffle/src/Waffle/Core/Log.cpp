#include "wfpch.h"
#include "Waffle/Core/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <spdlog/sinks/base_sink.h>

namespace Waffle {

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	Log::LogCallbackFn Log::s_LogCallback = nullptr;

	class CustomLogSink : public spdlog::sinks::base_sink<std::mutex>
	{
	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			// Do not leak TRACE logs or internal core "WAFFLE" engine logs to the in-editor console
			if (msg.level == spdlog::level::trace)
				return;
			if (msg.logger_name == "WAFFLE")
				return;

			spdlog::memory_buf_t formatted;
			spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
			std::string str(formatted.data(), formatted.size());
			// Remove trailing newline if present
			if (!str.empty() && (str.back() == '\n' || str.back() == '\r'))
				str.pop_back();
			if (!str.empty() && (str.back() == '\n' || str.back() == '\r'))
				str.pop_back();

			if (Log::GetLogCallback())
				Log::GetLogCallback()((int)msg.level, str);
		}

		void flush_() override {}
	};

	void Log::SetLogCallback(LogCallbackFn callback)
	{
		s_LogCallback = callback;
	}

	Log::LogCallbackFn Log::GetLogCallback()
	{
		return s_LogCallback;
	}

	void Log::Init()
	{
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Waffle.log", true));
		logSinks.emplace_back(std::make_shared<CustomLogSink>());

		logSinks[0]->set_pattern("%^[%T] %n: %v%$"); // Color, timestamp, name of logger(core, client), message
		logSinks[1]->set_pattern("[%T] [%l] %n: %v"); // Timestamp, level, name of logger, message
		logSinks[2]->set_pattern("%v"); // Raw message for callback

		s_CoreLogger = std::make_shared<spdlog::logger>("WAFFLE", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::trace);

		s_ClientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);
	}
}