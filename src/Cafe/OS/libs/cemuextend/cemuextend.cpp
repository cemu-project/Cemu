#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/cemuextend.h"
#include "Cafe/OS/libs/cemuextend/BridgeHost.h"
#include "Cafe/OS/libs/cemuextend/BuildId.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/cemuextend/Cex2Owner.h"
#include "Cafe/OS/libs/cemuextend/Cex2Storage.h"

#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/HW/Espresso/CemodRuntime.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "cemuextend/transport.hpp"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

namespace cemuextend_hle
{
	namespace
	{
		constexpr const char* kLibraryName = "cemuextend";

		template<ModMemoryPermission Permission>
		std::byte* ResolveGuest(PPCInterpreter_t* hCPU, uint32 address, uint32 size)
		{
			if (!address || !size)
				return nullptr;
			if (hCPU->modExecutionContext)
				return hCPU->modExecutionContext->Resolve(address, size, Permission);
			if (!GetCemodRuntime().TrustedOwner() || !memory_isAddressRangeAccessible(address, size))
				return nullptr;
			return reinterpret_cast<std::byte*>(memory_getPointerFromVirtualOffset(address));
		}

		Cex2Owner* CurrentOwner(PPCInterpreter_t* hCPU)
		{
			if (hCPU->modExecutionContext) return hCPU->modExecutionContext;
			auto* owner = GetCemodRuntime().TrustedOwner();
			return owner && owner->TitleId() == CafeSystem::GetForegroundTitleId() && !owner->IsStopped() ? owner : nullptr;
		}

		void Return(PPCInterpreter_t* hCPU, cemuextend::wire::Error result)
		{
			osLib_returnFromFunction(hCPU, static_cast<uint32>(static_cast<sint32>(result)));
		}

		void CEX2Query(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			if (hCPU->gpr[5] < sizeof(transport::Info))
				return Return(hCPU, wire::Error::InvalidArgument);
			auto* output = ResolveGuest<ModMemoryPermission::Write>(hCPU, hCPU->gpr[4], sizeof(transport::Info));
			if (!output)
				return Return(hCPU, wire::Error::InvalidArgument);
			std::array<std::byte, sizeof(transport::Info)> hostOutput{};
			const auto result = static_cast<wire::Error>(Cex2Host::Instance().Query(
				*owner, hCPU->gpr[3], hostOutput));
			if (result == wire::Error::Ok)
				std::memcpy(output, hostOutput.data(), hostOutput.size());
			Return(hCPU, result);
		}

		void CEX2Open(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			if (hCPU->gpr[4] != sizeof(transport::OpenOptions))
				return Return(hCPU, wire::Error::InvalidArgument);
			auto* options = ResolveGuest<ModMemoryPermission::Read>(hCPU, hCPU->gpr[3], hCPU->gpr[4]);
			auto* output = ResolveGuest<ModMemoryPermission::Write>(hCPU, hCPU->gpr[5], sizeof(wire::Be32));
			if (!options || !output)
				return Return(hCPU, wire::Error::InvalidArgument);
			transport::OpenOptions hostOptions{};
			std::memcpy(&hostOptions, options, sizeof(hostOptions));
			uint32 session{};
			const auto result = static_cast<wire::Error>(Cex2Host::Instance().Open(
				*owner,
				{reinterpret_cast<const std::byte*>(&hostOptions), sizeof(hostOptions)}, session));
			if (result == wire::Error::Ok)
			{
				const wire::Be32 encodedSession{session};
				std::memcpy(output, &encodedSession, sizeof(encodedSession));
			}
			Return(hCPU, result);
		}

		void CEX2Submit(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			if (hCPU->gpr[5] > transport::kMaximumMessageSize)
				return Return(hCPU, wire::Error::TooLarge);
			auto* request = ResolveGuest<ModMemoryPermission::Read>(hCPU, hCPU->gpr[4], hCPU->gpr[5]);
			if (!request)
				return Return(hCPU, wire::Error::InvalidArgument);
			std::vector<std::byte> hostRequest(hCPU->gpr[5]);
			std::memcpy(hostRequest.data(), request, hostRequest.size());
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Submit(
				*owner, hCPU->gpr[3], hostRequest)));
		}

		void CEX2Poll(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			if (hCPU->gpr[5] > transport::kMaximumMessageSize)
				return Return(hCPU, wire::Error::TooLarge);
			auto* output = ResolveGuest<ModMemoryPermission::Write>(hCPU, hCPU->gpr[4], hCPU->gpr[5]);
			auto* sizeOutput = ResolveGuest<ModMemoryPermission::Write>(hCPU, hCPU->gpr[6], sizeof(wire::Be32));
			if (!output || !sizeOutput)
				return Return(hCPU, wire::Error::InvalidArgument);
			std::vector<std::byte> hostOutput(hCPU->gpr[5]);
			uint32 outputSize{};
			const auto result = static_cast<wire::Error>(Cex2Host::Instance().Poll(
				*owner, hCPU->gpr[3], hostOutput, outputSize));
			if (result == wire::Error::Ok && outputSize <= hostOutput.size())
				std::memcpy(output, hostOutput.data(), outputSize);
			const wire::Be32 encodedSize{outputSize};
			std::memcpy(sizeOutput, &encodedSize, sizeof(encodedSize));
			Return(hCPU, result);
		}

		void CEX2Cancel(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Cancel(
				*owner, hCPU->gpr[3], hCPU->gpr[4])));
		}

		void CEX2Close(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			auto* owner = CurrentOwner(hCPU);
			if (!owner)
				return Return(hCPU, wire::Error::PermissionDenied);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Close(
				*owner, hCPU->gpr[3])));
		}

		sint32 CEXQuery(uint32, void*, uint32)
		{
			return static_cast<sint32>(cemuextend::wire::Error::AbiMismatch);
		}

		sint32 CEXRegister(uint32, void*, uint32, uint32be*)
		{
			return static_cast<sint32>(cemuextend::wire::Error::AbiMismatch);
		}

		sint32 CEXNotify(uint32, uint32)
		{
			return static_cast<sint32>(cemuextend::wire::Error::AbiMismatch);
		}

		sint32 CEXUnregister(uint32)
		{
			return static_cast<sint32>(cemuextend::wire::Error::AbiMismatch);
		}
	}

	class CemuExtendModule final : public COSModule
	{
	public:
		std::string_view GetName() override { return "cemuextend"; }

		void RPLMapped() override
		{
			cafeExportRegister("cemuextend", CEXQuery, LogType::Placeholder);
			cafeExportRegister("cemuextend", CEXRegister, LogType::Placeholder);
			cafeExportRegister("cemuextend", CEXNotify, LogType::Placeholder);
			cafeExportRegister("cemuextend", CEXUnregister, LogType::Placeholder);
			osLib_addFunctionInternal(kLibraryName, "CEX2Query", CEX2Query);
			osLib_addFunctionInternal(kLibraryName, "CEX2Open", CEX2Open);
			osLib_addFunctionInternal(kLibraryName, "CEX2Submit", CEX2Submit);
			osLib_addFunctionInternal(kLibraryName, "CEX2Poll", CEX2Poll);
			osLib_addFunctionInternal(kLibraryName, "CEX2Cancel", CEX2Cancel);
			osLib_addFunctionInternal(kLibraryName, "CEX2Close", CEX2Close);
			// Packages are activated after Graphic Packs so both systems share the
			// codecave allocator deterministically.
		}

		void RPLUnmapped() override
		{
			GetCemodRuntime().UnloadAll();
			Cex2Host::Instance().CloseAll();
		}

		void rpl_entry(uint32, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Unloaded)
			{
				GetCemodRuntime().UnloadAll();
				Cex2Host::Instance().CloseAll();
			}
		}
	};

	COSModule* GetModule()
	{
		static CemuExtendModule module;
		return &module;
	}

	void ConfigureCex2HleAccess(ModExecutionContext& context)
	{
		constexpr std::array names{"CEX2Query", "CEX2Open", "CEX2Submit", "CEX2Poll", "CEX2Cancel", "CEX2Close"};
		for (const auto* name : names)
		{
			const auto index = osLib_getFunctionIndex("cemuextend", name);
			if (index >= 0)
				context.AllowHle(static_cast<std::uint16_t>(index));
		}
	}

	CemodRuntime& GetCemodRuntime()
	{
		// Construct the session host first so the runtime is destroyed before it.
		(void)Cex2Host::Instance();
		static CemodRuntime runtime;
		return runtime;
	}

	std::vector<CemodPackageInfo> DiscoverCemods(std::uint64_t titleId)
	{
		namespace fs = std::filesystem;
		std::vector<CemodPackageInfo> result;
		if (titleId == 0) return result;
		const auto root = ActiveSettings::GetUserDataPath("cemuextend/mods");
		std::error_code error;
		fs::create_directories(root, error);
		if (error) return result;
		std::vector<fs::path> paths;
		for (fs::directory_iterator iterator(root, error), end; !error && iterator != end; ++iterator)
		{
			const auto status = iterator->symlink_status(error);
			if (error) break;
			if (fs::is_regular_file(status) && !fs::is_symlink(status) &&
				iterator->path().extension() == ".cemod") paths.push_back(iterator->path());
		}
		std::ranges::sort(paths);
		for (const auto& path : paths)
		{
			std::string packageError;
			auto package = CemodPackage::Load(path, titleId, packageError);
			if (!package)
			{
				result.push_back({path, {}, {}, 0, CemodExecutionMode::Isolated, false,
					std::move(packageError)});
				continue;
			}
			result.push_back({path, package->manifest.modId, package->principal,
				package->manifest.requestedPermissions, package->manifest.executionMode,
				package->signedPackage, {}});
		}
		return result;
	}

	void LoadCemodsForTitle(std::uint64_t titleId)
	{
		if (titleId == 0) return;
		auto& runtime = GetCemodRuntime();
		runtime.UnloadAll();
		const auto titleGrant = GetConfig().GetCemuExtendGrant(titleId).value_or(CemuExtendTitleGrant{
			kDefaultReadMask, kDefaultWriteMask, kDefaultInjectMask});
		const ModServicePermissions services{titleGrant.read_mask, titleGrant.write_mask,
			titleGrant.inject_mask};
		struct ApprovedPackage
		{
			CemodPackage package;
			std::filesystem::path path;
			std::uint32_t permissions{};
		};
		std::vector<ApprovedPackage> approved;
		std::uint32_t trustedPermissions{};
		for (const auto& info : DiscoverCemods(titleId))
		{
			if (!info.error.empty())
			{
				cemuLog_log(LogType::Force, "CemuExtend rejected cemod '{}': {}",
					_pathToUtf8(info.path), info.error);
				continue;
			}
			std::string error;
			auto package = CemodPackage::Load(info.path, titleId, error);
			if (!package) continue;
			const auto grant = GetConfig().GetCemuExtendModGrant(titleId, package->principal)
				.value_or(CemuExtendModGrant{});
			if (!grant.approved || (package->manifest.requestedPermissions & ~grant.approved_request_mask) != 0)
			{
				cemuLog_log(LogType::Force,
					"CemuExtend skipped unapproved cemod '{}' (approval or new permissions required)",
					_pathToUtf8(info.path));
				continue;
			}
			const auto permissions = grant.permissions & package->manifest.requestedPermissions;
			if (package->IsTrustedNative()) trustedPermissions |= permissions;
			approved.push_back({std::move(*package), info.path, permissions});
		}
		std::ranges::sort(approved, [](const ApprovedPackage& left, const ApprovedPackage& right) {
			if (left.package.manifest.modId != right.package.manifest.modId)
				return left.package.manifest.modId < right.package.manifest.modId;
			return left.path < right.path;
		});
		for (auto& item : approved)
		{
			if (runtime.Size() >= CemodRuntime::kMaximumModsPerTitle) break;
			std::string error;
			const auto effective = item.package.IsTrustedNative() ? trustedPermissions : item.permissions;
			if (!runtime.Load(std::move(item.package), effective, 0x1fU, error, &services))
				cemuLog_log(LogType::Force, "CemuExtend failed to load cemod '{}': {}",
					_pathToUtf8(item.path), error);
		}
		runtime.EventAll(1); // title loaded
	}

	void TickCemods()
	{
		GetCemodRuntime().TickAll();
	}

	void ReloadCemodPermissions(std::uint64_t titleId, std::string_view principal)
	{
		const auto titleGrant = GetConfig().GetCemuExtendGrant(titleId).value_or(CemuExtendTitleGrant{
			kDefaultReadMask, kDefaultWriteMask, kDefaultInjectMask});
		const ModServicePermissions services{titleGrant.read_mask, titleGrant.write_mask,
			titleGrant.inject_mask};
		const auto permissions = GetConfig().GetCemuExtendModGrant(titleId, principal)
			.value_or(CemuExtendModGrant{}).permissions;
		GetCemodRuntime().UpdatePermissions(principal, permissions, services);
		GetCemodRuntime().EventAll(2); // permissions changed
	}

	void ReloadCemodTitlePermissions(std::uint64_t titleId)
	{
		const auto titleGrant = GetConfig().GetCemuExtendGrant(titleId).value_or(CemuExtendTitleGrant{
			kDefaultReadMask, kDefaultWriteMask, kDefaultInjectMask});
		GetCemodRuntime().UpdateTitlePermissions({titleGrant.read_mask, titleGrant.write_mask,
			titleGrant.inject_mask});
		GetCemodRuntime().EventAll(2); // permissions changed
	}

	bool ImportLegacyData(std::uint64_t titleId, std::string_view principal, std::string& error)
	{
		const auto status = Cex2Storage::ImportLegacy(titleId, principal);
		if (status == cemuextend::wire::Status::Ok) { error.clear(); return true; }
		error = status == cemuextend::wire::Status::NotFound ? "No legacy title data was found." :
			status == cemuextend::wire::Status::Busy ? "The Mod already has ABI 2 data; nothing was overwritten." :
			"Legacy data failed validation or could not be copied.";
		return false;
	}
}
