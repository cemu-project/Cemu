#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/CemodPackage.h"
#include "Cafe/HW/Espresso/ModExecutionContext.h"

#include <openssl/evp.h>
#include <rapidjson/document.h>
#include <zip.h>

#include <charconv>
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace {

using Sha256 = std::array<unsigned char, 32>;

struct ZipCloser { void operator()(zip_t* value) const { if (value) zip_close(value); } };
struct ZipFileCloser { void operator()(zip_file_t* value) const { if (value) zip_fclose(value); } };
struct DigestCloser { void operator()(EVP_MD_CTX* value) const { EVP_MD_CTX_free(value); } };
struct KeyCloser { void operator()(EVP_PKEY* value) const { EVP_PKEY_free(value); } };

bool SafeEntryName(std::string_view name)
{
	if (name.empty() || name.size() > 255 || name.front() == '/' || name.front() == '\\' ||
		name.find('\\') != std::string_view::npos || name.find('\0') != std::string_view::npos)
		return false;
	std::size_t start{};
	while (start <= name.size())
	{
		const auto end = name.find('/', start);
		const auto component = name.substr(start, end == std::string_view::npos ? name.size() - start : end - start);
		if (component.empty() || component == "." || component == "..")
			return false;
		if (end == std::string_view::npos)
			break;
		start = end + 1;
	}
	return true;
}

bool ReadEntry(zip_t* archive, zip_uint64_t index, std::uint64_t maximum,
	std::vector<std::byte>& output)
{
	zip_stat_t stat{};
	if (zip_stat_index(archive, index, ZIP_FL_ENC_GUESS, &stat) != 0 ||
		(stat.valid & ZIP_STAT_SIZE) == 0 || stat.size > maximum ||
		((stat.valid & ZIP_STAT_ENCRYPTION_METHOD) != 0 && stat.encryption_method != ZIP_EM_NONE))
		return false;
	std::unique_ptr<zip_file_t, ZipFileCloser> file(zip_fopen_index(archive, index, ZIP_FL_UNCHANGED));
	if (!file)
		return false;
	output.resize(static_cast<std::size_t>(stat.size));
	std::size_t offset{};
	while (offset < output.size())
	{
		const auto read = zip_fread(file.get(), output.data() + offset, output.size() - offset);
		if (read <= 0)
			return false;
		offset += static_cast<std::size_t>(read);
	}
	return true;
}

Sha256 Hash(std::span<const std::byte> bytes)
{
	Sha256 result{};
	std::unique_ptr<EVP_MD_CTX, DigestCloser> context(EVP_MD_CTX_new());
	unsigned int size{};
	if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
		EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) != 1 ||
		EVP_DigestFinal_ex(context.get(), result.data(), &size) != 1 || size != result.size())
		result.fill(0);
	return result;
}

void AppendU32(std::vector<std::byte>& output, std::uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8)
		output.push_back(static_cast<std::byte>((value >> shift) & 0xff));
}

void AppendU64(std::vector<std::byte>& output, std::uint64_t value)
{
	for (int shift = 56; shift >= 0; shift -= 8)
		output.push_back(static_cast<std::byte>((value >> shift) & 0xff));
}

std::string Hex(std::span<const unsigned char> bytes)
{
	constexpr char digits[] = "0123456789abcdef";
	std::string result(bytes.size() * 2, '0');
	for (std::size_t index = 0; index < bytes.size(); ++index)
	{
		result[index * 2] = digits[bytes[index] >> 4];
		result[index * 2 + 1] = digits[bytes[index] & 15];
	}
	return result;
}

bool ParseTitleId(const rapidjson::Value& value, std::uint64_t& output)
{
	if (value.IsUint64())
	{
		output = value.GetUint64();
		return true;
	}
	if (!value.IsString() || value.GetStringLength() == 0 || value.GetStringLength() > 18)
		return false;
	std::string_view text(value.GetString(), value.GetStringLength());
	if (text.starts_with("0x") || text.starts_with("0X"))
		text.remove_prefix(2);
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), output, 16);
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool ParseManifest(std::span<const std::byte> bytes, CemodManifest& manifest, std::string& error)
{
	if (bytes.empty() || bytes.size() > 256U * 1024U)
	{
		error = "manifest.json has an invalid size";
		return false;
	}
	rapidjson::Document document;
	document.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (document.HasParseError() || !document.IsObject() ||
		!document.HasMember("package_version") || !document["package_version"].IsUint() ||
		document["package_version"].GetUint() != 1 || !document.HasMember("api_version") ||
		!document["api_version"].IsUint() || document["api_version"].GetUint() != 2 ||
		!document.HasMember("execution_mode") || !document["execution_mode"].IsString() ||
		!document.HasMember("mod_id") || !document["mod_id"].IsString() ||
		!document.HasMember("title_ids") || !document["title_ids"].IsArray() ||
		!document.HasMember("requested_permissions") || !document["requested_permissions"].IsArray())
	{
		error = "manifest.json does not match the CEX2 schema";
		return false;
	}
	manifest.packageVersion = document["package_version"].GetUint();
	manifest.apiVersion = document["api_version"].GetUint();
	const std::string_view executionMode(document["execution_mode"].GetString(),
		document["execution_mode"].GetStringLength());
	if (executionMode == "isolated")
		manifest.executionMode = CemodExecutionMode::Isolated;
	else if (executionMode == "trusted_native")
		manifest.executionMode = CemodExecutionMode::TrustedNative;
	else
	{
		error = "execution_mode must be isolated or trusted_native";
		return false;
	}
	manifest.modId.assign(document["mod_id"].GetString(), document["mod_id"].GetStringLength());
	if (manifest.modId.empty() || manifest.modId.size() > 128 ||
		!std::ranges::all_of(manifest.modId, [](unsigned char c) {
			return std::isalnum(c) || c == '.' || c == '_' || c == '-';
		}))
	{
		error = "mod_id is invalid";
		return false;
	}
	if (document["title_ids"].Empty() || document["title_ids"].Size() > 64)
	{
		error = "title_ids is empty or too large";
		return false;
	}
	for (const auto& value : document["title_ids"].GetArray())
	{
		std::uint64_t title{};
		if (!ParseTitleId(value, title) || title == 0)
		{
			error = "title_ids contains an invalid title ID";
			return false;
		}
		manifest.titleIds.push_back(title);
	}
	static const std::map<std::string_view, std::uint32_t> permissionBits{
		{"read", 1U}, {"write", 2U}, {"inject", 4U}, {"clipboard", 8U}, {"capture", 16U},
	};
	for (const auto& value : document["requested_permissions"].GetArray())
	{
		if (!value.IsString())
		{
			error = "requested_permissions contains a non-string value";
			return false;
		}
		const std::string_view name(value.GetString(), value.GetStringLength());
		const auto found = permissionBits.find(name);
		if (found == permissionBits.end() || (manifest.requestedPermissions & found->second) != 0)
		{
			error = "requested_permissions contains an unknown or duplicate value";
			return false;
		}
		manifest.requestedPermissions |= found->second;
	}
	if (manifest.executionMode == CemodExecutionMode::TrustedNative)
	{
		if (document.HasMember("memory") || document.HasMember("cpu") || document.HasMember("entrypoint"))
		{
			error = "trusted_native manifests must not contain isolated resource or lifecycle fields";
			return false;
		}
		manifest.codeBytes = 8U * 1024U * 1024U;
		return true;
	}
	if (!document.HasMember("memory") || !document["memory"].IsObject() ||
		!document.HasMember("cpu") || !document["cpu"].IsObject() ||
		!document.HasMember("entrypoint") || !document["entrypoint"].IsString())
	{
		error = "isolated manifests require memory, cpu, and entrypoint";
		return false;
	}
	const auto& memory = document["memory"];
	const auto& cpu = document["cpu"];
	if (!memory.HasMember("code_bytes") || !memory["code_bytes"].IsUint() ||
		!memory.HasMember("private_bytes") || !memory["private_bytes"].IsUint() ||
		!memory.HasMember("stack_bytes") || !memory["stack_bytes"].IsUint() ||
		!cpu.HasMember("instructions_per_frame") || !cpu["instructions_per_frame"].IsUint() ||
		!cpu.HasMember("time_us_per_frame") || !cpu["time_us_per_frame"].IsUint())
	{
		error = "memory or cpu limits are missing";
		return false;
	}
	manifest.codeBytes = memory["code_bytes"].GetUint();
	manifest.privateBytes = memory["private_bytes"].GetUint();
	manifest.stackBytes = memory["stack_bytes"].GetUint();
	manifest.instructionsPerFrame = cpu["instructions_per_frame"].GetUint();
	manifest.timeMicrosecondsPerFrame = cpu["time_us_per_frame"].GetUint();
	manifest.entrypoint.assign(document["entrypoint"].GetString(), document["entrypoint"].GetStringLength());
	if (manifest.codeBytes == 0 || manifest.codeBytes > ModExecutionContext::kMaximumCodeBytes ||
		manifest.privateBytes == 0 || manifest.privateBytes > ModExecutionContext::kMaximumPrivateBytes ||
		manifest.stackBytes == 0 || manifest.stackBytes > ModExecutionContext::kMaximumStackBytes ||
		manifest.stackBytes % ModExecutionContext::kPageSize != 0 ||
		manifest.instructionsPerFrame == 0 ||
		manifest.instructionsPerFrame > ModExecutionContext::kMaximumInstructionsPerFrame ||
		manifest.timeMicrosecondsPerFrame == 0 || manifest.timeMicrosecondsPerFrame > 1000 ||
		manifest.entrypoint != "cemod_init")
	{
		error = "manifest resource limits or entrypoint are invalid";
		return false;
	}
	return true;
}

bool ValidateElf(std::span<const std::byte> elf, const CemodManifest& manifest, std::string& error)
{
	const auto maximumElfBytes = manifest.executionMode == CemodExecutionMode::TrustedNative ?
		CemodPackage::kMaximumExpandedBytes :
		static_cast<std::uint64_t>(manifest.codeBytes) + manifest.privateBytes;
	if (elf.size() < 52 || elf.size() > maximumElfBytes)
	{
		error = "PPC ELF has an invalid size";
		return false;
	}
	const auto* data = reinterpret_cast<const unsigned char*>(elf.data());
	auto u16 = [data](std::size_t offset) { return static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]); };
	auto u32 = [data](std::size_t offset) { return (static_cast<std::uint32_t>(data[offset]) << 24) |
		(static_cast<std::uint32_t>(data[offset + 1]) << 16) |
		(static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3]; };
	if (data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F' ||
		data[4] != 1 || data[5] != 2 || data[6] != 1 || u16(18) != 20 || u32(20) != 1 ||
		(manifest.executionMode == CemodExecutionMode::TrustedNative && u16(16) != 3))
	{
		error = "package executable is not a 32-bit big-endian PPC ELF";
		return false;
	}
	const auto programOffset = u32(28);
	const auto programEntrySize = u16(42);
	const auto programCount = u16(44);
	if (programCount == 0 || programCount > 128 || programEntrySize < 32 ||
		programOffset > elf.size() || static_cast<std::uint64_t>(programCount) * programEntrySize > elf.size() - programOffset)
	{
		error = "PPC ELF program table is out of bounds";
		return false;
	}
	std::uint64_t codeBytes{};
	std::uint64_t dataBytes{};
	struct LoadRange { std::uint32_t address; std::uint32_t size; std::uint32_t flags; };
	std::vector<LoadRange> loadRanges;
	for (std::uint16_t index = 0; index < programCount; ++index)
	{
		const auto offset = programOffset + static_cast<std::uint32_t>(index) * programEntrySize;
		if (u32(offset) != 1)
			continue;
		const auto fileOffset = u32(offset + 4);
		const auto fileSize = u32(offset + 16);
		const auto memorySize = u32(offset + 20);
		const auto flags = u32(offset + 24);
		if (fileSize > memorySize || fileOffset > elf.size() || fileSize > elf.size() - fileOffset ||
			((flags & 2U) != 0 && (flags & 1U) != 0))
		{
			error = "PPC ELF contains an invalid or writable-executable segment";
			return false;
		}
		if ((flags & 1U) != 0)
			codeBytes += memorySize;
		else
			dataBytes += memorySize;
		loadRanges.push_back({u32(offset + 8), memorySize, flags});
	}
	if (codeBytes == 0 ||
		(manifest.executionMode == CemodExecutionMode::Isolated &&
			(codeBytes > manifest.codeBytes || dataBytes > manifest.privateBytes)) ||
		(manifest.executionMode == CemodExecutionMode::TrustedNative &&
			codeBytes + dataBytes > 8U * 1024U * 1024U))
	{
		error = "PPC ELF exceeds the manifest memory limits";
		return false;
	}
	if (manifest.executionMode == CemodExecutionMode::TrustedNative)
	{
		struct Section
		{
			std::uint32_t name{}, type{}, flags{}, address{}, offset{}, size{}, link{}, entrySize{};
		};
		const auto sectionOffset = u32(32);
		const auto sectionEntrySize = u16(46);
		const auto sectionCount = u16(48);
		const auto sectionNames = u16(50);
		if (sectionEntrySize < 40 || sectionCount == 0 || sectionCount > 1024 ||
			sectionNames >= sectionCount || sectionOffset > elf.size() ||
			static_cast<std::uint64_t>(sectionCount) * sectionEntrySize > elf.size() - sectionOffset)
		{
			error = "trusted ELF section table is invalid";
			return false;
		}
		std::vector<Section> sections;
		sections.reserve(sectionCount);
		for (std::uint16_t index = 0; index < sectionCount; ++index)
		{
			const auto offset = sectionOffset + static_cast<std::uint32_t>(index) * sectionEntrySize;
			Section section{u32(offset), u32(offset + 4), u32(offset + 8), u32(offset + 12),
				u32(offset + 16), u32(offset + 20), u32(offset + 24), u32(offset + 36)};
			if (section.type != 8 && (section.offset > elf.size() || section.size > elf.size() - section.offset))
			{
				error = "trusted ELF section is out of bounds";
				return false;
			}
			sections.push_back(section);
		}
		const auto& names = sections[sectionNames];
		if (names.type != 3 || names.offset > elf.size() || names.size > elf.size() - names.offset)
		{
			error = "trusted ELF section-name table is invalid";
			return false;
		}
		auto sectionName = [&](const Section& section) -> std::optional<std::string_view> {
			if (section.name >= names.size) return std::nullopt;
			const auto* value = reinterpret_cast<const char*>(elf.data() + names.offset + section.name);
			const auto maximum = names.size - section.name;
			const auto length = strnlen(value, maximum);
			if (length == maximum) return std::nullopt;
			return std::string_view(value, length);
		};
		auto imageContains = [&](std::uint32_t address, std::uint32_t size, bool executable) {
			return std::ranges::any_of(loadRanges, [&](const LoadRange& range) {
				return (!executable || (range.flags & 1U) != 0) && address >= range.address &&
				address - range.address <= range.size && size <= range.size - (address - range.address);
			});
		};
		std::optional<std::size_t> bootstrap;
		static constexpr std::array<std::uint8_t, 8> allowedRelocations{0, 1, 4, 5, 6, 10, 22, 26};
		for (std::size_t index = 0; index < sections.size(); ++index)
		{
			const auto& section = sections[index];
			const auto name = sectionName(section);
			if (!name)
			{
				error = "trusted ELF contains an invalid section name";
				return false;
			}
			if (*name == ".cemod.bootstrap")
			{
				if (bootstrap) { error = "trusted ELF contains duplicate bootstrap sections"; return false; }
				bootstrap = index;
			}
			if (section.type == 9)
			{
				error = "trusted ELF must use RELA relocations";
				return false;
			}
			if (section.type == 4)
			{
				if (section.entrySize < 12 || section.size % section.entrySize != 0 ||
					section.link >= sections.size() ||
					(sections[section.link].type != 2 && sections[section.link].type != 11))
				{
					error = "trusted ELF relocation table is invalid";
					return false;
				}
				const auto& symbols = sections[section.link];
				if (symbols.entrySize < 16 || symbols.size % symbols.entrySize != 0)
				{
					error = "trusted ELF relocation symbol table is invalid";
					return false;
				}
				for (std::uint32_t entry = 0; entry < section.size / section.entrySize; ++entry)
				{
					const auto relocation = section.offset + entry * section.entrySize;
					const auto info = u32(relocation + 4);
					const auto type = static_cast<std::uint8_t>(info);
					if (std::ranges::find(allowedRelocations, type) == allowedRelocations.end() ||
						(info >> 8) >= symbols.size / symbols.entrySize)
					{
						error = "trusted ELF contains an unsupported relocation";
						return false;
					}
					const auto width = (type == 4 || type == 5 || type == 6) ? 2U : 4U;
					if (type != 0 && !imageContains(u32(relocation), width, false))
					{
						error = "trusted ELF relocation target is outside the image";
						return false;
					}
				}
			}
			if (section.type == 2 || section.type == 11)
			{
				if (section.entrySize < 16 || section.size % section.entrySize != 0)
				{
					error = "trusted ELF symbol table is invalid";
					return false;
				}
				for (std::uint32_t symbol = 1; symbol < section.size / section.entrySize; ++symbol)
					if (u16(section.offset + symbol * section.entrySize + 14) == 0)
					{
						error = "trusted ELF contains an undefined symbol";
						return false;
					}
			}
		}
		if (!bootstrap)
		{
			error = "trusted ELF is missing .cemod.bootstrap";
			return false;
		}
		const auto& table = sections[*bootstrap];
		if (table.type != 1 || (table.flags & 2U) == 0 || table.size < 12 ||
			!imageContains(table.address, table.size, false) || u32(table.offset) != 0x434d4231U ||
			u16(table.offset + 4) != 1 || u16(table.offset + 6) != 24)
		{
			error = "trusted ELF contains an invalid CMB1 bootstrap section";
			return false;
		}
		const auto recordCount = u32(table.offset + 8);
		if (recordCount == 0 || recordCount > 64 || table.size != 12U + recordCount * 24U)
		{
			error = "trusted ELF contains an invalid CMB1 record count";
			return false;
		}
		for (std::uint32_t record = 0; record < recordCount; ++record)
		{
			const auto offset = table.offset + 12 + record * 24;
			if (u32(offset) == 0 || (u32(offset + 4) & 3U) != 0 || u32(offset + 12) == 0 ||
				u32(offset + 20) != 0 || !imageContains(u32(offset + 16), 4, true))
			{
				error = "trusted ELF contains an invalid CMB1 record";
				return false;
			}
		}
	}
	return true;
}

} // namespace

std::optional<CemodPackage> CemodPackage::Inspect(const std::filesystem::path& path,
	std::string& error)
{
	error.clear();
	if (path.extension() != ".cemod")
	{
		error = "package must use the .cemod extension";
		return std::nullopt;
	}
	std::error_code filesystemError;
	const auto packageSize = std::filesystem::file_size(path, filesystemError);
	if (filesystemError || packageSize == 0 || packageSize > kMaximumExpandedBytes)
	{
		error = "package file has an invalid size";
		return std::nullopt;
	}
	int zipError{};
	std::unique_ptr<zip_t, ZipCloser> archive(zip_open(path.string().c_str(), ZIP_RDONLY, &zipError));
	if (!archive)
	{
		error = "package is not a readable ZIP container";
		return std::nullopt;
	}
	const auto count = zip_get_num_entries(archive.get(), 0);
	if (count <= 0 || count > 256)
	{
		error = "package contains an invalid number of entries";
		return std::nullopt;
	}
	std::map<std::string, std::vector<std::byte>> entries;
	std::uint64_t expandedBytes{};
	for (zip_int64_t index = 0; index < count; ++index)
	{
		const char* rawName = zip_get_name(archive.get(), index, ZIP_FL_ENC_STRICT);
		if (!rawName || !SafeEntryName(rawName) || std::string_view(rawName).ends_with('/'))
		{
			error = "package contains an unsafe entry name";
			return std::nullopt;
		}
		std::vector<std::byte> data;
		if (!ReadEntry(archive.get(), index, kMaximumExpandedBytes - expandedBytes, data))
		{
			error = "package entry cannot be read or exceeds the expansion limit";
			return std::nullopt;
		}
		expandedBytes += data.size();
		if (!entries.emplace(rawName, std::move(data)).second)
		{
			error = "package contains a duplicate entry name";
			return std::nullopt;
		}
	}
	const auto manifestEntry = entries.find("manifest.json");
	const auto elfEntry = entries.find("mod.elf");
	if (manifestEntry == entries.end() || elfEntry == entries.end())
	{
		error = "package must contain manifest.json and mod.elf";
		return std::nullopt;
	}
	CemodPackage result;
	if (!ParseManifest(manifestEntry->second, result.manifest, error) ||
		!ValidateElf(elfEntry->second, result.manifest, error))
	{
		return std::nullopt;
	}
	result.elf = elfEntry->second;

	const auto signature = entries.find("signature.ed25519");
	const auto publicKey = entries.find("public_key.ed25519");
	if ((signature == entries.end()) != (publicKey == entries.end()))
	{
		error = "signature and public key must either both be present or both be absent";
		return std::nullopt;
	}
	if (signature != entries.end())
	{
		if (signature->second.size() != 64 || publicKey->second.size() != 32)
		{
			error = "Ed25519 signature material has an invalid size";
			return std::nullopt;
		}
		std::vector<std::byte> canonical;
		for (const auto& [name, data] : entries)
		{
			if (name == "signature.ed25519")
				continue;
			AppendU32(canonical, static_cast<std::uint32_t>(name.size()));
			canonical.insert(canonical.end(), reinterpret_cast<const std::byte*>(name.data()),
				reinterpret_cast<const std::byte*>(name.data() + name.size()));
			AppendU64(canonical, data.size());
			const auto digest = Hash(data);
			canonical.insert(canonical.end(), reinterpret_cast<const std::byte*>(digest.data()),
				reinterpret_cast<const std::byte*>(digest.data() + digest.size()));
		}
		const auto digest = Hash(canonical);
		std::unique_ptr<EVP_PKEY, KeyCloser> key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
			reinterpret_cast<const unsigned char*>(publicKey->second.data()), publicKey->second.size()));
		std::unique_ptr<EVP_MD_CTX, DigestCloser> verify(EVP_MD_CTX_new());
		if (!key || !verify || EVP_DigestVerifyInit(verify.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
			EVP_DigestVerify(verify.get(), reinterpret_cast<const unsigned char*>(signature->second.data()),
				signature->second.size(), digest.data(), digest.size()) != 1)
		{
			error = "Ed25519 signature verification failed";
			return std::nullopt;
		}
		const auto fingerprint = Hash(publicKey->second);
		result.principal = "ed25519:" + Hex(fingerprint) + ":" + result.manifest.modId;
		result.signedPackage = true;
	}
	else
	{
		std::ifstream file(path, std::ios::binary);
		std::vector<std::byte> packageBytes(static_cast<std::size_t>(packageSize));
		if (!file.read(reinterpret_cast<char*>(packageBytes.data()), packageBytes.size()))
		{
			error = "package cannot be hashed";
			return std::nullopt;
		}
		const auto digest = Hash(packageBytes);
		result.principal = "sha256:" + Hex(digest);
	}
	return result;
}

std::optional<CemodPackage> CemodPackage::Load(const std::filesystem::path& path,
	std::uint64_t titleId, std::string& error)
{
	auto result = Inspect(path, error);
	if (!result) return std::nullopt;
	if (titleId == 0 || std::ranges::find(result->manifest.titleIds, titleId) == result->manifest.titleIds.end())
	{
		error = "package does not target the active title";
		return std::nullopt;
	}
	result->targetTitleId = titleId;
	return result;
}
