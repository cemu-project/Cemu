#pragma once

#include <cstdint>
#include <string_view>

namespace cemuextend_hle
{
	constexpr std::uint64_t HashBuildId(std::string_view commit)
	{
		std::uint64_t value = 14695981039346656037ULL;
		for (const auto character : commit)
		{
			value ^= static_cast<std::uint8_t>(character);
			value *= 1099511628211ULL;
		}
		return value;
	}

	inline constexpr std::uint64_t kCemuExtendBuildId = HashBuildId(CEMU_EXTEND_COMMIT_HASH);
}
