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
struct CemuExtendModGrant;

struct CemodPackageInfo
{
	std::filesystem::path path;
	std::string modId;
	std::string principal;
	std::uint32_t requestedPermissions{};
	CemodExecutionMode executionMode{CemodExecutionMode::Isolated};
	bool signedPackage{};
	std::vector<std::uint64_t> titleIds;
	std::string error;
};

struct CemodPermissionRequest
{
	std::string modId;
	std::string principal;
	std::uint32_t requestedPermissions{};
	std::uint32_t grantedPermissions{};
	CemodExecutionMode executionMode{CemodExecutionMode::Isolated};
	bool signedPackage{};
};

namespace cemuextend_hle
{
	COSModule* GetModule();
	void ConfigureCex2HleAccess(ModExecutionContext& context);
	CemodRuntime& GetCemodRuntime();
	::CemuExtendModGrant ResolveCemodGrant(std::uint64_t titleId, const std::string& modId,
		const std::string& principal, std::uint32_t requestedPermissions);
	std::vector<CemodPackageInfo> DiscoverCemodCatalog();
	std::vector<CemodPackageInfo> DiscoverCemods(std::uint64_t titleId);
	std::vector<CemodPermissionRequest> PendingCemodPermissionRequests(std::uint64_t titleId);
	void LoadCemodsForTitle(std::uint64_t titleId);
	void TickCemods();
	void ReloadCemodPermissions(std::uint64_t titleId, std::string_view principal);
	void ReloadCemodTitlePermissions(std::uint64_t titleId);
	bool ImportLegacyData(std::uint64_t titleId, std::string_view principal, std::string& error);
}
