#pragma once

#include "Cafe/HW/Espresso/CemodPackage.h"
#include "Cafe/OS/RPL/COSModule.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class ModExecutionContext;
class CemodRuntime;

struct CemodPackageInfo
{
	std::filesystem::path path;
	std::string modId;
	std::string principal;
	std::uint32_t requestedPermissions{};
	CemodExecutionMode executionMode{CemodExecutionMode::Isolated};
	bool signedPackage{};
	std::string error;
};

namespace cemuextend_hle
{
	COSModule* GetModule();
	void ConfigureCex2HleAccess(ModExecutionContext& context);
	CemodRuntime& GetCemodRuntime();
	std::vector<CemodPackageInfo> DiscoverCemods(std::uint64_t titleId);
	void LoadCemodsForTitle(std::uint64_t titleId);
	void TickCemods();
	void ReloadCemodPermissions(std::uint64_t titleId, std::string_view principal);
	void ReloadCemodTitlePermissions(std::uint64_t titleId);
	bool ImportLegacyData(std::uint64_t titleId, std::string_view principal, std::string& error);
}
