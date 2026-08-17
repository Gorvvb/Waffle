#include "wfpch.h"
#include "UUID.h"

#include <random>

namespace Waffle {

	static uint64_t GenerateUUID64()
	{
		thread_local std::random_device s_RandomDevice;
		thread_local std::mt19937_64 s_Engine(s_RandomDevice());
		thread_local std::uniform_int_distribution<uint64_t> s_UniformDistribution;

		return s_UniformDistribution(s_Engine);
	}

	static uint32_t GenerateUUID32()
	{
		thread_local std::random_device s_RandomDevice;
		thread_local std::mt19937 s_Engine(s_RandomDevice());
		thread_local std::uniform_int_distribution<uint32_t> s_UniformDistribution;

		return s_UniformDistribution(s_Engine);
	}

	UUID::UUID()
		: m_UUID(GenerateUUID64())
	{
	}

	UUID::UUID(uint64_t uuid)
		: m_UUID(uuid)
	{
	}

	UUID32::UUID32()
		: m_UUID(GenerateUUID32())
	{
	}

	UUID32::UUID32(uint32_t uuid)
		: m_UUID(uuid)
	{
	}
}