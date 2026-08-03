#pragma once

#include "Waffle/Core/Base.h"

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Waffle {

	class JobSystem
	{
	public:
		static void Init(uint32_t threadCount = 0);
		static void Shutdown();

		static void Execute(const std::function<void()>& job);
		static void Wait();
		static bool IsBusy();

		static uint32_t GetWorkerThreadCount() { return s_ThreadCount; }

	private:
		static void WorkerLoop();

	private:
		static std::vector<std::thread> s_Workers;
		static std::queue<std::function<void()>> s_JobQueue;
		static std::mutex s_QueueMutex;
		static std::condition_variable s_Condition;
		static std::condition_variable s_WaitCondition;
		static std::atomic<uint32_t> s_ActiveJobs;
		static std::atomic<bool> s_Shutdown;
		static uint32_t s_ThreadCount;
	};

}
