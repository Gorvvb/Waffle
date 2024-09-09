#pragma once

#include <xhash>

namespace Waffle {

	class UUID
	{
	private:
		uint64_t m_UUID; // Maybe change to a 128 bit integer
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }
	};
}

namespace std {

	template<>
	struct hash<Waffle::UUID>
	{
		std::size_t operator()(const Waffle::UUID& uuid) const
		{
			return hash<uint64_t>()((uint64_t)uuid);
		}
	};
}