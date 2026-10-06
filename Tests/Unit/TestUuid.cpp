#include "TestHarness.h"

#include "Waffle/Core/UUID.h"

#include <unordered_set>

WTEST(uuid_generated_values_are_unique_and_nonzero)
{
	std::unordered_set<uint64_t> seen;
	constexpr int kCount = 10000;
	seen.reserve(kCount);

	for (int i = 0; i < kCount; i++)
	{
		uint64_t uuid = (uint64_t)Waffle::UUID();
		EXPECT_TRUE(uuid != 0, "generated UUID must never be 0 (reserved value)");
		EXPECT_TRUE(seen.insert(uuid).second, "generated UUIDs must be unique");
	}
}

WTEST(uuid_wraps_an_explicit_value)
{
	Waffle::UUID uuid(0x1234567890ABCDEFull);
	EXPECT_TRUE((uint64_t)uuid == 0x1234567890ABCDEFull, "explicit value round-trips");
}

WTEST(uuid_hash_is_the_raw_value)
{
	Waffle::UUID uuid(42);
	std::hash<Waffle::UUID> hasher;
	EXPECT_TRUE(hasher(uuid) == 42, "hash must be the raw value (m_EntityMap relies on it)");
}
