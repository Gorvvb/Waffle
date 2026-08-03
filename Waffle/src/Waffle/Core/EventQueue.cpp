#include "wfpch.h"
#include "EventQueue.h"

namespace Waffle {

	std::vector<Scope<Event>> EventQueue::s_EventQueue;
	std::mutex EventQueue::s_QueueMutex;

	void EventQueue::Init()
	{
		std::lock_guard<std::mutex> lock(s_QueueMutex);
		s_EventQueue.clear();
	}

	void EventQueue::Shutdown()
	{
		std::lock_guard<std::mutex> lock(s_QueueMutex);
		s_EventQueue.clear();
	}

	void EventQueue::DispatchPendingEvents(const EventDispatchCallback& callback)
	{
		if (!callback) return;

		std::vector<Scope<Event>> pendingBuffer;
		{
			std::lock_guard<std::mutex> lock(s_QueueMutex);
			pendingBuffer.swap(s_EventQueue);
		}

		for (auto& event : pendingBuffer)
		{
			if (event)
			{
				callback(*event);
			}
		}
	}

	size_t EventQueue::GetPendingEventCount()
	{
		std::lock_guard<std::mutex> lock(s_QueueMutex);
		return s_EventQueue.size();
	}

}
