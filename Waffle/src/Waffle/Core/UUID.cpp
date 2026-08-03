#include "wfpch.h"
#include "UUID.h"

#include <random>

namespace Waffle {

	static uint64_t GenerateUUID()
	{
		thread_local std::random_device s_RandomDevice;
		thread_local std::mt19937_64 s_Engine(s_RandomDevice());
		thread_local std::uniform_int_distribution<uint64_t> s_UniformDistribution;

		return s_UniformDistribution(s_Engine);
	}

	UUID::UUID()
		: m_UUID(GenerateUUID())
	{

	}

	UUID::UUID(uint64_t uuid)
		: m_UUID(uuid)
	{

	}
}