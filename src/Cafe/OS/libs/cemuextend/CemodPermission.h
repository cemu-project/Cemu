#pragma once

#include <cstdint>

namespace cemuextend_hle
{
	constexpr std::uint32_t kCemodPermissionMask = 0x1fU;

	[[nodiscard]] constexpr bool NeedsCemodPermissionPrompt(std::uint32_t requested,
		std::uint32_t granted, std::uint32_t approvedRequests, bool enabled)
	{
		requested &= kCemodPermissionMask;
		return enabled && ((requested & ~granted) != 0 ||
			(requested & ~approvedRequests) != 0);
	}
}
