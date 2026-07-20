#pragma once

#include <cstdint>
#include <string>

namespace cemuextend_hle
{
	// Stable host-side identity for a CEX2 security boundary. Isolated Mods use
	// one owner per sandbox; trusted native Mods deliberately share one owner for
	// the active title.
	class Cex2Owner
	{
	public:
		virtual ~Cex2Owner() = default;
		[[nodiscard]] virtual std::uint64_t AddressSpaceId() const = 0;
		[[nodiscard]] virtual std::uint32_t Generation() const = 0;
		[[nodiscard]] virtual const std::string& Principal() const = 0;
		[[nodiscard]] virtual std::uint64_t TitleId() const = 0;
		[[nodiscard]] virtual bool IsStopped() const = 0;
		[[nodiscard]] virtual std::uint32_t GrantedPermissions() const = 0;
		virtual void SetGrantedPermissions(std::uint32_t permissions) = 0;
		[[nodiscard]] virtual bool IsServiceAllowed(std::uint16_t service,
			std::uint32_t permission, std::uint16_t operation = 0) const = 0;
	};
}
