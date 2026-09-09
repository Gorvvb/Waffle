#include "wfpch.h"
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
#if defined(WF_PLATFORM_WINDOWS)
		localtime_s(&time_info, &in_time_t);
		ss << std::put_time(&time_info, "%H:%M:%S");
#else
		(void)time_info;
		ss << std::put_time(localtime(&in_time_t), "%H:%M:%S");
#endif

		s_Messages.push_back({ level, message, ss.str() });
		if (s_Messages.size() > 1000)
			s_Messages.erase(s_Messages.begin());
	}

	void ConsolePanel::OnImGuiRender()
	{
		ImGui::Begin("Console");

		if (ImGui::Button("Clear"))
		{
			std::lock_guard<std::mutex> lock(s_MessageMutex);
			s_Messages.clear();
		}

		ImGui::SameLine();
		static bool showTrace = true, showInfo = true, showWarn = true, showError = true, showCritical = true;
		ImGui::Checkbox("Trace", &showTrace); ImGui::SameLine();
		ImGui::Checkbox("Info", &showInfo); ImGui::SameLine();
		ImGui::Checkbox("Warn", &showWarn); ImGui::SameLine();
		ImGui::Checkbox("Error", &showError); ImGui::SameLine();
		ImGui::Checkbox("Critical", &showCritical);

		ImGui::Separator();

		ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		// Copy under lock, render outside it - holding the mutex across up to
		// 1000 Text calls stalled producer threads for a frame under heavy
		// logging.
		std::vector<ConsoleMessage> snapshot;
		{
			std::lock_guard<std::mutex> lock(s_MessageMutex);
			snapshot = s_Messages;
		}

		for (const auto& msg : snapshot)
		{
			if (msg.LogLevel == ConsoleMessage::Level::Trace && !showTrace) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Info && !showInfo) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Warn && !showWarn) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Error && !showError) continue;
			if (msg.LogLevel == ConsoleMessage::Level::Critical && !showCritical) continue;

			ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			switch (msg.LogLevel)
			{
				case ConsoleMessage::Level::Trace:    color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); break;
				case ConsoleMessage::Level::Info:     color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); break;
				case ConsoleMessage::Level::Warn:     color = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); break;
				case ConsoleMessage::Level::Error:    color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); break;
				case ConsoleMessage::Level::Critical: color = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); break;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::Text("[%s] %s", msg.Timestamp.c_str(), msg.Message.c_str());
			ImGui::PopStyleColor();
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}

}
