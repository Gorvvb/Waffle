#include "wfpch.h"
#include "Memory.h"

namespace Waffle {

	static AllocationStats s_GlobalStats;

	namespace Memory
	{
		const AllocationStats& GetAllocationStats()
		{
			return s_GlobalStats;
		}
	}

	void Allocator::Init()
	{
		if (!s_Data)
			s_Data = new AllocatorData();
	}

	void Allocator::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}

	void* Allocator::AllocateRaw(size_t size)
	{
		return std::malloc(size);
	}

	void* Allocator::Allocate(size_t size)
	{
		return Allocate(size, "Uncategorized");
	}

	void* Allocator::Allocate(size_t size, const char* desc)
	{
		void* memory = std::malloc(size);
		if (s_Data)
		{
			std::lock_guard<std::mutex> lock(s_Data->m_Mutex);
			s_Data->m_AllocationMap[memory] = { memory, size, desc, 0 };
			s_Data->m_AllocationStatsMap[desc].TotalAllocated += size;
		}
		s_GlobalStats.TotalAllocated += size;
		return memory;
	}

	void* Allocator::Allocate(size_t size, const char* file, int line)
	{
		return Allocate(size, file);
	}

	void* Allocator::AllocateAligned(size_t size, size_t alignment)
	{
		return AllocateAligned(size, alignment, "Aligned");
	}

	void* Allocator::AllocateAligned(size_t size, size_t alignment, const char* desc)
	{
#if defined(_MSC_VER)
		void* memory = _aligned_malloc(size, alignment);
#else
		void* memory = nullptr;
		posix_memalign(&memory, alignment, size);
#endif
		if (s_Data)
		{
			std::lock_guard<std::mutex> lock(s_Data->m_Mutex);
			s_Data->m_AllocationMap[memory] = { memory, size, desc, alignment };
			s_Data->m_AllocationStatsMap[desc].TotalAllocated += size;
		}
		s_GlobalStats.TotalAllocated += size;
		return memory;
	}

	void Allocator::Free(void* memory)
	{
		if (!memory) return;
		if (s_Data)
		{
			std::lock_guard<std::mutex> lock(s_Data->m_Mutex);
			auto it = s_Data->m_AllocationMap.find(memory);
			if (it != s_Data->m_AllocationMap.end())
			{
				s_Data->m_AllocationStatsMap[it->second.Category].TotalFreed += it->second.Size;
				s_GlobalStats.TotalFreed += it->second.Size;
				s_Data->m_AllocationMap.erase(it);
			}
		}
		std::free(memory);
	}

	void Allocator::Free(void* memory, size_t size)
	{
		Free(memory);
	}

	void Allocator::FreeAligned(void* memory)
	{
		if (!memory) return;
		if (s_Data)
		{
			std::lock_guard<std::mutex> lock(s_Data->m_Mutex);
			auto it = s_Data->m_AllocationMap.find(memory);
			if (it != s_Data->m_AllocationMap.end())
			{
				s_Data->m_AllocationStatsMap[it->second.Category].TotalFreed += it->second.Size;
				s_GlobalStats.TotalFreed += it->second.Size;
				s_Data->m_AllocationMap.erase(it);
			}
		}
#if defined(_MSC_VER)
		_aligned_free(memory);
#else
		std::free(memory);
#endif
	}

	void Allocator::FreeAligned(void* memory, size_t size)
	{
		FreeAligned(memory);
	}

	const AllocatorData::AllocationStatsMap& Allocator::GetAllocationStats()
	{
		static AllocatorData::AllocationStatsMap emptyMap;
		return s_Data ? s_Data->m_AllocationStatsMap : emptyMap;
	}

}
