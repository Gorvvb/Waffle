#pragma once

#include "Waffle/Core/Base.h"
#include "Waffle/Events/Event.h"

#include <vector>
#include <mutex>
#include <functional>

namespace Waffle {

	class EventQueue
	{
	public:
		using EventDispatchCallback = std::function<void(Event&)>;

		static void Init();
		static void Shutdown();

		template<typename T, typename... Args>
		static void PostEvent(Args&&... args)
		{
			static_assert(std::is_base_of<Event, T>::value, "T must derive from Event");
			std::lock_guard<std::mutex> lock(s_QueueMutex);
			s_EventQueue.push_back(CreateScope<T>(std::forward<Args>(args)...));
		}

		static void PostEvent(Scope<Event> event)
		{
			if (!event) return;
			std::lock_guard<std::mutex> lock(s_QueueMutex);
			s_EventQueue.push_back(std::move(event));
		}

		static void DispatchPendingEvents(const EventDispatchCallback& callback);
		static size_t GetPendingEventCount();

	private:
		static std::vector<Scope<Event>> s_EventQueue;
		static std::mutex s_QueueMutex;
	};

}
