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

	UUID::UUID()
	{
		uint64_t value = GenerateUUID64();
		// 0 is the reserved "no id" value (RelationshipComponent::Parent uses it as null) - never hand it out.
		m_UUID = value ? value : 1;
	}

	UUID::UUID(uint64_t uuid)
		: m_UUID(uuid)
	{
	}

}