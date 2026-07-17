#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace CemuPatchBinaryV2 {

inline constexpr std::uint32_t kMagic = 0x43504232U;
inline constexpr std::uint16_t kVersion = 2;
inline constexpr std::uint16_t kHeaderSize = 48;

std::vector<std::uint8_t> Generate(std::span<const std::uint8_t> cpb1Payload,
	std::uint32_t featureFlags = 0);
bool Verify(std::span<const std::uint8_t> file, std::span<const std::uint8_t>& cpb1Payload,
	std::uint32_t& featureFlags, std::string& error);

} // namespace CemuPatchBinaryV2
