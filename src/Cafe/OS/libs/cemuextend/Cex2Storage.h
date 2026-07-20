#pragma once

#include "cemuextend/services.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cemuextend_hle {

struct Cex2StorageResult
{
	cemuextend::wire::Status status{cemuextend::wire::Status::IoError};
	std::vector<std::byte> payload;
};

// Host-owned, principal-scoped storage used only by ABI 2. Every request is
// decoded from a copied message and every result is returned as copied bytes.
class Cex2Storage
{
public:
	static bool ValidateConfigurationFormat(std::span<const std::byte> bytes);
	static Cex2StorageResult Dispatch(std::uint64_t titleId, std::string_view principal,
		cemuextend::wire::ServiceId service, std::uint16_t operation,
		std::span<const std::byte> payload);
	static cemuextend::wire::Status ImportLegacy(std::uint64_t titleId,
		std::string_view principal);
};

} // namespace cemuextend_hle
