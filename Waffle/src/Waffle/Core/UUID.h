#pragma once

#include <cstdint>
#include <functional>

namespace Waffle {

	// Persistent entity identity: a random 64-bit value, serialized with every entity and stable
	// across save/load. Full-width mt19937_64 keeps accidental collisions negligible (~5 billion
	// entities for 1-in-a-million odds), and Scene::CreateEntityWithUUID additionally verifies
	// uniqueness against the live registry, so a corrupt file can never shadow an existing entity.
	// Note: this is NOT the runtime entity handle - entt slot handles (with generation bits) are
	// recycled at runtime and never serialized.
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

}