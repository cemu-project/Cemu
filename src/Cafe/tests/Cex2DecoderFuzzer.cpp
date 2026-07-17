#include "cemuextend/services.hpp"
#include "cemuextend/transport.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	using namespace cemuextend;
	if (size < sizeof(transport::RequestHeader) || size > transport::kMaximumMessageSize) return 0;
	transport::RequestHeader header{};
	std::memcpy(&header, data, sizeof(header));
	if (header.totalSize.get() != size || header.flags.get() != 0) return 0;
	wire::Decoder decoder({reinterpret_cast<const std::byte*>(data + sizeof(header)),
		size - sizeof(header)});
	std::string text; std::uint64_t offset{}; std::uint32_t length{};
	switch (static_cast<wire::ServiceId>(header.serviceId.get()))
	{
	case wire::ServiceId::Configuration:
		decoder.String(text); break;
	case wire::ServiceId::File:
		decoder.String(text); decoder.U64(offset); decoder.U32(length); break;
	case wire::ServiceId::Logging:
		{ std::uint8_t level{}; decoder.U8(level); decoder.String(text); break; }
	default: break;
	}
	return 0;
}
