#pragma once

#include <cstdint>
#include <functional>

namespace Waffle {

	class UUID
	{
	private:
		uint64_t m_UUID;
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }
	};

	class UUID32
	{
	private:
		uint32_t m_UUID;
	public:
		UUID32();
		UUID32(uint32_t uuid);
		UUID32(const UUID32&) = default;

		operator uint32_t() const { return m_UUID; }
	};
}

namespace std {

	template<>
	struct hash<Waffle::UUID>
	{
		std::size_t operator()(const Waffle::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};

	template<>
	struct hash<Waffle::UUID32>
	{
		std::size_t operator()(const Waffle::UUID32& uuid) const
		{
			return (uint32_t)uuid;
		}
	};
}