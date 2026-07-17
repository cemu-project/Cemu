#include "Cafe/GraphicPack/CemuPatchBinaryV2.h"

#include <cstdint>
#include <span>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	std::span<const std::uint8_t> payload;
	std::uint32_t flags{};
	std::string error;
	(void)CemuPatchBinaryV2::Verify({data, size}, payload, flags, error);
	return 0;
}
