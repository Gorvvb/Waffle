#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace Waffle {

	struct ConsoleMessage
	{
		enum class Level { Trace = 0, Info, Warn, Error, Critical };

		Level       LogLevel;
		std::string Message;
		std::string Timestamp;
	};

	class ConsolePanel
	{
	public:
		ConsolePanel();
		~ConsolePanel() = default;

		static void AddMessage(ConsoleMessage::Level level, const std::string& message);

		void OnImGuiRender();

	private:
		static std::vector<ConsoleMessage> s_Messages;
		static std::mutex                  s_MessageMutex;

		bool m_AutoScroll = true;
		bool m_ShowInfo = true;
		bool m_ShowWarn = true;
		bool m_ShowError = true;
		char m_FilterBuffer[256] = "";
	};

}
