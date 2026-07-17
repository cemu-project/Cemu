#include "Cafe/GraphicPack/CemuPatchBinaryV2.h"

#include <openssl/sha.h>

#include <array>
#include <cstring>
#include <limits>

namespace CemuPatchBinaryV2 {
namespace {

void Put16(std::uint8_t* output, std::uint16_t value)
{
	output[0] = value >> 8; output[1] = value;
}
void Put32(std::uint8_t* output, std::uint32_t value)
{
	output[0] = value >> 24; output[1] = value >> 16; output[2] = value >> 8; output[3] = value;
}
std::uint16_t Get16(const std::uint8_t* input) { return (input[0] << 8) | input[1]; }
std::uint32_t Get32(const std::uint8_t* input)
{
	return (std::uint32_t{input[0]} << 24) | (std::uint32_t{input[1]} << 16) |
		(std::uint32_t{input[2]} << 8) | input[3];
}

} // namespace

std::vector<std::uint8_t> Generate(std::span<const std::uint8_t> payload, std::uint32_t flags)
{
	if (payload.empty() || payload.size() > std::numeric_limits<std::uint32_t>::max()) return {};
	std::vector<std::uint8_t> output(kHeaderSize + payload.size());
	Put32(output.data(), kMagic); Put16(output.data() + 4, kVersion); Put16(output.data() + 6, kHeaderSize);
	Put32(output.data() + 8, flags); Put32(output.data() + 12, static_cast<std::uint32_t>(payload.size()));
	SHA256(payload.data(), payload.size(), output.data() + 16);
	std::memcpy(output.data() + kHeaderSize, payload.data(), payload.size());
	return output;
}

bool Verify(std::span<const std::uint8_t> file, std::span<const std::uint8_t>& payload,
	std::uint32_t& flags, std::string& error)
{
	payload = {}; flags = 0;
	if (file.size() < kHeaderSize || Get32(file.data()) != kMagic || Get16(file.data() + 4) != kVersion ||
		Get16(file.data() + 6) != kHeaderSize)
	{ error = "invalid CPB2 header"; return false; }
	flags = Get32(file.data() + 8);
	if (flags != 0) { error = "unsupported CPB2 feature flags"; return false; }
	const auto size = Get32(file.data() + 12);
	if (size == 0 || size != file.size() - kHeaderSize) { error = "invalid CPB2 payload length"; return false; }
	std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
	SHA256(file.data() + kHeaderSize, size, digest.data());
	if (std::memcmp(digest.data(), file.data() + 16, digest.size()) != 0)
	{ error = "CPB2 payload SHA-256 mismatch"; return false; }
	payload = file.subspan(kHeaderSize); error.clear(); return true;
}

} // namespace CemuPatchBinaryV2
