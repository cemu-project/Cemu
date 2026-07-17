#include "Cafe/GraphicPack/CemuPatchBinaryV2.h"

#include <array>
#include <cstdlib>
#include <iostream>

#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed at " << __LINE__ << '\n'; std::abort(); } } while (false)

int main()
{
	const std::array payload{std::uint8_t{'C'}, std::uint8_t{'P'}, std::uint8_t{'B'}, std::uint8_t{'1'}, std::uint8_t{0}};
	auto file = CemuPatchBinaryV2::Generate(payload);
	CHECK(!file.empty()); std::span<const std::uint8_t> decoded; std::uint32_t flags{}; std::string error;
	CHECK(CemuPatchBinaryV2::Verify(file, decoded, flags, error)); CHECK(decoded.size() == payload.size());
	file.back() ^= 1; CHECK(!CemuPatchBinaryV2::Verify(file, decoded, flags, error));
	return 0;
}
