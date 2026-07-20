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

std::vector<std::byte> TrustedElf(bool writableExecutable = false, bool missingBootstrap = false,
	std::optional<std::uint8_t> relocationType = std::nullopt)
{
	constexpr std::uint32_t namesOffset = 84;
	constexpr std::string_view names{"\0.shstrtab\0.cemod.bootstrap\0.dynsym\0.rela.dyn\0", 47};
	constexpr std::uint32_t bootstrapOffset = 132;
	constexpr std::uint32_t symbolOffset = 168;
	constexpr std::uint32_t relocationOffset = 184;
	constexpr std::uint32_t sectionsOffset = 196;
	const std::uint16_t sectionCount = relocationType ? 5 : 3;
	std::vector<std::byte> elf(sectionsOffset + sectionCount * 40);
	elf[0] = std::byte{0x7f}; elf[1] = std::byte{'E'}; elf[2] = std::byte{'L'}; elf[3] = std::byte{'F'};
	elf[4] = std::byte{1}; elf[5] = std::byte{2}; elf[6] = std::byte{1};
	Be16(elf, 16, 3); Be16(elf, 18, 20); Be32(elf, 20, 1);
	Be32(elf, 28, 52); Be32(elf, 32, sectionsOffset);
	Be16(elf, 40, 52); Be16(elf, 42, 32); Be16(elf, 44, 1);
	Be16(elf, 46, 40); Be16(elf, 48, sectionCount); Be16(elf, 50, 1);
	Be32(elf, 52, 1); Be32(elf, 56, 0); Be32(elf, 60, 0); Be32(elf, 64, 0);
	Be32(elf, 68, elf.size()); Be32(elf, 72, elf.size());
	Be32(elf, 76, writableExecutable ? 7 : 5); Be32(elf, 80, 16);
	std::memcpy(elf.data() + namesOffset, names.data(), names.size());
	Be32(elf, bootstrapOffset, 0x434d4231); Be16(elf, bootstrapOffset + 4, 1);
	Be16(elf, bootstrapOffset + 6, 24); Be32(elf, bootstrapOffset + 8, 1);
	Be32(elf, bootstrapOffset + 12, 0x867317de); Be32(elf, bootstrapOffset + 16, 0x02f37154);
	Be32(elf, bootstrapOffset + 20, 0x4e800421); Be32(elf, bootstrapOffset + 24, 0xffffffff);
	Be32(elf, bootstrapOffset + 28, 0); Be32(elf, bootstrapOffset + 32, 0);
	const auto namesSection = sectionsOffset + 40;
	Be32(elf, namesSection + 4, 3); Be32(elf, namesSection + 16, namesOffset);
	Be32(elf, namesSection + 20, names.size()); Be32(elf, namesSection + 32, 1);
	const auto bootstrapSection = sectionsOffset + 80;
	Be32(elf, bootstrapSection, missingBootstrap ? 1 : 11); Be32(elf, bootstrapSection + 4, 1);
	Be32(elf, bootstrapSection + 8, 2); Be32(elf, bootstrapSection + 12, bootstrapOffset);
	Be32(elf, bootstrapSection + 16, bootstrapOffset); Be32(elf, bootstrapSection + 20, 36);
	Be32(elf, bootstrapSection + 32, 4);
	if (relocationType)
	{
		const auto symbolSection = sectionsOffset + 120;
		Be32(elf, symbolSection, 28); Be32(elf, symbolSection + 4, 11);
		Be32(elf, symbolSection + 8, 2); Be32(elf, symbolSection + 16, symbolOffset);
		Be32(elf, symbolSection + 20, 16); Be32(elf, symbolSection + 24, 1);
		Be32(elf, symbolSection + 32, 4); Be32(elf, symbolSection + 36, 16);
		const auto relocationSection = sectionsOffset + 160;
		Be32(elf, relocationSection, 36); Be32(elf, relocationSection + 4, 4);
		Be32(elf, relocationSection + 8, 2); Be32(elf, relocationSection + 16, relocationOffset);
		Be32(elf, relocationSection + 20, 12); Be32(elf, relocationSection + 24, 3);
		Be32(elf, relocationSection + 32, 4); Be32(elf, relocationSection + 36, 12);
		Be32(elf, relocationOffset, bootstrapOffset + 28);
		Be32(elf, relocationOffset + 4, *relocationType);
	}
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

constexpr std::string_view kIsolatedManifest = R"({
 "package_version":1,
 "api_version":2,
 "execution_mode":"isolated",
 "mod_id":"org.example.safe",
 "title_ids":["0005000012345678"],
 "requested_permissions":["read"],
 "memory":{"code_bytes":4096,"private_bytes":4096,"stack_bytes":4096},
 "cpu":{"instructions_per_frame":100000,"time_us_per_frame":500},
 "entrypoint":"cemod_init"
})";

constexpr std::string_view kTrustedManifest = R"({
 "package_version":1,
 "api_version":2,
 "execution_mode":"trusted_native",
 "mod_id":"org.example.native",
 "title_ids":["0005000012345678"],
 "requested_permissions":["read","write"]
})";

void WritePackage(const std::filesystem::path& path, bool unsafe,
	std::string_view manifest = kIsolatedManifest, std::vector<std::byte> elf = Elf(),
	bool invalidSignature = false)
{
	std::filesystem::remove(path);
	int error{};
	auto* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_EXCL, &error);
	CHECK(archive != nullptr);
	Add(archive, "manifest.json", manifest.data(), manifest.size());
	Add(archive, "mod.elf", elf.data(), elf.size());
	if (unsafe)
		Add(archive, "../escape", manifest.data(), 1);
	if (invalidSignature)
	{
		static constexpr std::byte value{1};
		Add(archive, "public_key.ed25519", &value, 1);
		Add(archive, "signature.ed25519", &value, 1);
	}
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
	auto inspected = CemodPackage::Inspect(path, error);
	CHECK(inspected.has_value());
	CHECK(inspected->targetTitleId == 0);
	CHECK(inspected->manifest.titleIds == std::vector<std::uint64_t>{0x0005000012345678ULL});
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

void TestLegacyManifestRejected()
{
	const auto path = PackagePath("legacy-manifest");
	constexpr std::string_view legacy = R"({"api_version":2,"mod_id":"old","title_ids":["0005000012345678"],"requested_permissions":[]})";
	WritePackage(path, false, legacy);
	std::string error;
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error));
	CHECK(error == "manifest.json does not match the CEX2 schema");
	std::filesystem::remove(path);
}

void TestTrustedValidation()
{
	std::string error;
	auto path = PackagePath("trusted-valid");
	WritePackage(path, false, kTrustedManifest, TrustedElf());
	auto package = CemodPackage::Load(path, 0x0005000012345678ULL, error);
	CHECK(package && package->IsTrustedNative());
	std::filesystem::remove(path);

	path = PackagePath("trusted-none-relocation");
	WritePackage(path, false, kTrustedManifest, TrustedElf(false, false, 0));
	CHECK(CemodPackage::Load(path, 0x0005000012345678ULL, error));
	std::filesystem::remove(path);

	path = PackagePath("trusted-wx");
	WritePackage(path, false, kTrustedManifest, TrustedElf(true));
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error));
	CHECK(error == "PPC ELF contains an invalid or writable-executable segment");
	std::filesystem::remove(path);

	path = PackagePath("trusted-bootstrap");
	WritePackage(path, false, kTrustedManifest, TrustedElf(false, true));
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error));
	CHECK(error == "trusted ELF is missing .cemod.bootstrap");
	std::filesystem::remove(path);

	path = PackagePath("trusted-relocation");
	WritePackage(path, false, kTrustedManifest, TrustedElf(false, false, 2));
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error));
	CHECK(error == "trusted ELF contains an unsupported relocation");
	std::filesystem::remove(path);

	path = PackagePath("bad-signature");
	WritePackage(path, false, kTrustedManifest, TrustedElf(), true);
	CHECK(!CemodPackage::Load(path, 0x0005000012345678ULL, error));
	CHECK(error == "Ed25519 signature material has an invalid size");
	std::filesystem::remove(path);
}

} // namespace

int main()
{
	TestUnsignedPrincipalAndValidation();
	TestUnsafeEntryRejected();
	TestLegacyManifestRejected();
	TestTrustedValidation();
	return 0;
}
