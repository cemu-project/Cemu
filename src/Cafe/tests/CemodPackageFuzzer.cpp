#include "Cafe/HW/Espresso/CemodPackage.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (size > 64U * 1024U * 1024U)
		return 0;
	static std::mutex mutex;
	std::lock_guard lock(mutex);
	const auto path = std::filesystem::temp_directory_path() / "cemuextend-package-fuzzer.cemod";
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
	}
	std::string error;
	(void)CemodPackage::Load(path, 1, error);
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
	return 0;
}
