#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class CemodExecutionMode : std::uint8_t
{
	Isolated,
	TrustedNative,
};

struct CemodManifest
{
	std::uint32_t packageVersion{};
	std::uint32_t apiVersion{};
	CemodExecutionMode executionMode{CemodExecutionMode::Isolated};
	std::string modId;
	std::vector<std::uint64_t> titleIds;
	std::uint32_t requestedPermissions{};
	std::uint32_t codeBytes{};
	std::uint32_t privateBytes{};
	std::uint32_t stackBytes{};
	std::uint32_t instructionsPerFrame{};
	std::uint32_t timeMicrosecondsPerFrame{};
	std::string entrypoint;
};

struct CemodPackage
{
	static constexpr std::uint64_t kMaximumExpandedBytes = 64ULL * 1024ULL * 1024ULL;

	CemodManifest manifest;
	std::vector<std::byte> elf;
	std::string principal;
	std::uint64_t targetTitleId{};
	bool signedPackage{};

	[[nodiscard]] bool IsTrustedNative() const
	{
		return manifest.executionMode == CemodExecutionMode::TrustedNative;
	}

	[[nodiscard]] static std::optional<CemodPackage> Load(const std::filesystem::path& path,
		std::uint64_t titleId, std::string& error);
};
