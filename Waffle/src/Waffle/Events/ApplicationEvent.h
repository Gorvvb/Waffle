#pragma once

#include "Event.h"
#include <filesystem>
#include <vector>

namespace Waffle {

	class WindowDropEvent : public Event
	{
	public:
		WindowDropEvent(const std::vector<std::filesystem::path>& paths)
			: m_Paths(paths) {}

		const std::vector<std::filesystem::path>& GetPaths() const { return m_Paths; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowDropEvent: " << m_Paths.size() << " files";
			return ss.str();
		}

		EVENT_CLASS_TYPE(WindowDrop)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		std::vector<std::filesystem::path> m_Paths;
	};

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_Width(width), m_Height(height) {}

		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}

		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		unsigned int m_Width, m_Height;
	};

	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() {}

		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class AppTickEvent : public Event
	{
	public:
		AppTickEvent() {}

		EVENT_CLASS_TYPE(AppTick)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class AppUpdateEvent : public Event
	{
	public:
		AppUpdateEvent() {}

		EVENT_CLASS_TYPE(AppUpdate)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class AppRenderEvent : public Event
	{
	public:
		AppRenderEvent() {}

		EVENT_CLASS_TYPE(AppRender)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};
}