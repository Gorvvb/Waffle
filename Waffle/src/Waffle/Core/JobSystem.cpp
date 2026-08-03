#include "wfpch.h"
#include "JobSystem.h"
#include "Waffle/Core/Log.h"

namespace Waffle {

	std::vector<std::thread> JobSystem::s_Workers;
	std::queue<std::function<void()>> JobSystem::s_JobQueue;
	std::mutex JobSystem::s_QueueMutex;
	std::condition_variable JobSystem::s_Condition;
	std::condition_variable JobSystem::s_WaitCondition;
	std::atomic<uint32_t> JobSystem::s_ActiveJobs{ 0 };
	std::atomic<bool> JobSystem::s_Shutdown{ false };
	uint32_t JobSystem::s_ThreadCount = 0;

	void JobSystem::Init(uint32_t threadCount)
	{
		if (!s_Workers.empty()) return;

		s_Shutdown = false;
		s_ActiveJobs = 0;

		uint32_t hardwareThreads = std::thread::hardware_concurrency();
		s_ThreadCount = (threadCount > 0) ? threadCount : ((hardwareThreads > 1) ? hardwareThreads - 1 : 1);

		WF_CORE_INFO("JobSystem initialized with {0} worker threads", s_ThreadCount);

		for (uint32_t i = 0; i < s_ThreadCount; ++i)
		{
			s_Workers.emplace_back(WorkerLoop);
		}
	}

	void JobSystem::Shutdown()
	{
		if (s_Workers.empty()) return;

		{
			std::lock_guard<std::mutex> lock(s_QueueMutex);
			s_Shutdown = true;
		}
		s_Condition.notify_all();

		for (auto& worker : s_Workers)
		{
			if (worker.joinable())
				worker.join();
		}

		s_Workers.clear();
		std::queue<std::function<void()>> emptyQueue;
		std::swap(s_JobQueue, emptyQueue);

		WF_CORE_INFO("JobSystem shutdown complete");
	}

	void JobSystem::Execute(const std::function<void()>& job)
	{
		if (!job) return;

		{
			std::lock_guard<std::mutex> lock(s_QueueMutex);
			s_JobQueue.push(job);
			s_ActiveJobs++;
		}

		s_Condition.notify_one();
	}

	void JobSystem::Wait()
	{
		std::unique_lock<std::mutex> lock(s_QueueMutex);
		s_WaitCondition.wait(lock, []() {
			return s_JobQueue.empty() && s_ActiveJobs == 0;
		});
	}

	bool JobSystem::IsBusy()
	{
		return s_ActiveJobs > 0;
	}

	void JobSystem::WorkerLoop()
	{
		while (true)
		{
			std::function<void()> job;

			{
				std::unique_lock<std::mutex> lock(s_QueueMutex);
				s_Condition.wait(lock, []() {
					return s_Shutdown || !s_JobQueue.empty();
				});

				if (s_Shutdown && s_JobQueue.empty())
					return;

				job = std::move(s_JobQueue.front());
				s_JobQueue.pop();
			}

			job();

			s_ActiveJobs--;
			s_WaitCondition.notify_all();
		}
	}

}
