#include "Cafe/OS/libs/cemuextend/Cex2Storage.h"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	(void)cemuextend_hle::Cex2Storage::ValidateConfigurationFormat(
		{reinterpret_cast<const std::byte*>(data), size});
	return 0;
}
