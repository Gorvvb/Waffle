#include "ConsolePanel.h"

#include <imgui/imgui.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace Waffle {

	std::vector<ConsoleMessage> ConsolePanel::s_Messages;
	std::mutex ConsolePanel::s_MessageMutex;

	ConsolePanel::ConsolePanel()
	{
	}

	void ConsolePanel::AddMessage(ConsoleMessage::Level level, const std::string& message)
	{
		std::lock_guard<std::mutex> lock(s_MessageMutex);

		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		std::stringstream ss;
		tm time_info;
#if defined(_WIN32)
		localtime_s(&time_info, &in_time_t);
		ss << std::put_time(&time_info, "%H:%M:%S");
#else
		ss << std::put_time(localtime(&in_time_t), "%H:%M:%S");
#endif

		s_Messages.push_back({ level, message, ss.str() });
		if (s_Messages.size() > 1000)
			s_Messages.erase(s_Messages.begin());
	}

	void ConsolePanel::OnImGuiRender()
	{
		ImGui::Begin("Console");

		// Top controls toolbar
		if (ImGui::Button("Clear"))
		{
			std::lock_guard<std::mutex> lock(s_MessageMutex);
			s_Messages.clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::Checkbox("Info", &m_ShowInfo);
		ImGui::SameLine();
		ImGui::Checkbox("Warn", &m_ShowWarn);
		ImGui::SameLine();
		ImGui::Checkbox("Error", &m_ShowError);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputText("Search", m_FilterBuffer, sizeof(m_FilterBuffer));

		ImGui::Separator();

		// Message list child window
		ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		std::lock_guard<std::mutex> lock(s_MessageMutex);

		std::string filterStr = m_FilterBuffer;
		for (auto& c : filterStr) c = (char)tolower(c);

		for (const auto& msg : s_Messages)
		{
			if (msg.LogLevel == ConsoleMessage::Level::Info && !m_ShowInfo) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Trace && !m_ShowInfo) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Warn && !m_ShowWarn) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Error && !m_ShowError) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Critical && !m_ShowError) continue;

			if (!filterStr.empty())
			{
				std::string msgLower = msg.Message;
				for (auto& c : msgLower) c = (char)tolower(c);
				if (msgLower.find(filterStr) == std::string::npos)
					continue;
			}

			ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Default Info/Trace
			const char* prefix = "[INFO]";

			switch (msg.LogLevel)
			{
			case ConsoleMessage::Level::Trace:
				color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
				prefix = "[TRACE]";
				break;
			case ConsoleMessage::Level::Info:
				color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
				prefix = "[INFO]";
				break;
			case ConsoleMessage::Level::Warn:
				color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
				prefix = "[WARN]";
				break;
			case ConsoleMessage::Level::Error:
				color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
				prefix = "[ERROR]";
				break;
			case ConsoleMessage::Level::Critical:
				color = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
				prefix = "[FATAL]";
				break;
			}

			ImGui::TextDisabled("[%s]", msg.Timestamp.c_str());
			ImGui::SameLine();
			ImGui::TextColored(color, "%s %s", prefix, msg.Message.c_str());
		}

		if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}

}
