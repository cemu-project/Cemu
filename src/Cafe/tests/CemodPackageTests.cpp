#include "Cafe/HW/Espresso/CemodPackage.h"

#include <zip.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void CheckFailed(const char* expression, int line)
{
	std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
	std::abort();
}
#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

void Be16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value >> 8);
	bytes[offset + 1] = static_cast<std::byte>(value);
}

void Be32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
	bytes[offset] = static_cast<std::byte>(value >> 24);
	bytes[offset + 1] = static_cast<std::byte>(value >> 16);
	bytes[offset + 2] = static_cast<std::byte>(value >> 8);
	bytes[offset + 3] = static_cast<std::byte>(value);
}

std::vector<std::byte> Elf()
{
	std::vector<std::byte> elf(88);
	elf[0] = std::byte{0x7f}; elf[1] = std::byte{'E'}; elf[2] = std::byte{'L'}; elf[3] = std::byte{'F'};
	elf[4] = std::byte{1}; elf[5] = std::byte{2}; elf[6] = std::byte{1};
	Be16(elf, 16, 2); Be16(elf, 18, 20); Be32(elf, 20, 1); Be32(elf, 24, 0x10000000);
	Be32(elf, 28, 52); Be16(elf, 40, 52); Be16(elf, 42, 32); Be16(elf, 44, 1);
	Be32(elf, 52, 1); Be32(elf, 56, 84); Be32(elf, 60, 0x10000000); Be32(elf, 64, 0x10000000);
	Be32(elf, 68, 4); Be32(elf, 72, 4); Be32(elf, 76, 5); Be32(elf, 80, 4096);
	Be32(elf, 84, 0x4e800020); // blr
	return elf;
}

void Add(zip_t* archive, const char* name, const void* data, std::size_t size)
{
	auto* source = zip_source_buffer(archive, data, size, 0);
	CHECK(source != nullptr);
	CHECK(zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8) >= 0);
}

std::filesystem::path PackagePath(std::string_view suffix)
{
	return std::filesystem::temp_directory_path() /
		("cemuextend-package-test-" + std::to_string(static_cast<unsigned long long>(std::hash<std::string_view>{}(suffix))) + ".cemod");
}

void WritePackage(const std::filesystem::path& path, bool unsafe)
{
	std::filesystem::remove(path);
	int error{};
	auto* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_EXCL, &error);
	CHECK(archive != nullptr);
	constexpr std::string_view manifest = R"({
 "api_version":2,
 "mod_id":"org.example.safe",
 "title_ids":["0005000012345678"],
 "requested_permissions":["read"],
 "memory":{"code_bytes":4096,"private_bytes":4096,"stack_bytes":4096},
 "cpu":{"instructions_per_frame":100000,"time_us_per_frame":500},
 "entrypoint":"cemod_init"
})";
	const auto elf = Elf();
	Add(archive, "manifest.json", manifest.data(), manifest.size());
	Add(archive, "mod.elf", elf.data(), elf.size());
	if (unsafe)
		Add(archive, "../escape", manifest.data(), 1);
	CHECK(zip_close(archive) == 0);
}

void TestUnsignedPrincipalAndValidation()
{
	const auto path = PackagePath("valid");
	WritePackage(path, false);
	std::string error;
	auto package = CemodPackage::Load(path, 0x0005000012345678ULL, error);
	CHECK(package.has_value());
	CHECK(package->manifest.modId == "org.example.safe");
	CHECK(package->principal.starts_with("sha256:"));
	CHECK(!package->signedPackage);
	CHECK(!CemodPackage::Load(path, 0x0005000099999999ULL, error).has_value());
	CHECK(error == "package does not target the active title");
	std::filesystem::remove(path);
}

void TestUnsafeEntryRejected()
{
	const auto path = PackagePath("unsafe");
	WritePackage(path, true);
	std::string error;
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error).has_value());
	CHECK(error == "package contains an unsafe entry name");
	std::filesystem::remove(path);
}

} // namespace

int main()
{
	TestUnsignedPrincipalAndValidation();
	TestUnsafeEntryRejected();
	return 0;
}
