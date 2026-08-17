#pragma once

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <utility>
#include <new>

namespace Waffle {

	struct AllocationStats
	{
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	struct Allocation
	{
		void* Memory = nullptr;
		size_t Size = 0;
		const char* Category = nullptr;
		size_t Alignment = 0;
	};

	namespace Memory
	{
		const AllocationStats& GetAllocationStats();
	}

	struct AllocatorData
	{
		using AllocationStatsMap = std::map<const char*, AllocationStats>;

		std::map<const void*, Allocation> m_AllocationMap;
		AllocationStatsMap m_AllocationStatsMap;
		std::mutex m_Mutex;
	};

	class Allocator
	{
	public:
		static void Init();
		static void Shutdown();

		static void* AllocateRaw(size_t size);
		static void* Allocate(size_t size);
		static void* Allocate(size_t size, const char* desc);
		static void* Allocate(size_t size, const char* file, int line);

		static void* AllocateAligned(size_t size, size_t alignment);
		static void* AllocateAligned(size_t size, size_t alignment, const char* desc);

		static void Free(void* memory);
		static void Free(void* memory, size_t size);
		static void FreeAligned(void* memory);
		static void FreeAligned(void* memory, size_t size);

		static const AllocatorData::AllocationStatsMap& GetAllocationStats();
	private:
		inline static AllocatorData* s_Data = nullptr;
	};

}
