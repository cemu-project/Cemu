#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/cemuextend.h"
#include "Cafe/OS/libs/cemuextend/BridgeHost.h"

#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"

namespace cemuextend_hle
{
	namespace
	{
		constexpr uint64 kBuildId = 0x4345585400010000ULL;

		bool ValidateGuestRange(void* pointer, uint32 size)
		{
			if (!pointer || size == 0)
				return false;
			const auto address = memory_getVirtualOffsetFromPointer(pointer);
			if (address > std::numeric_limits<uint32>::max() - size)
				return false;
			return memory_isAddressRangeAccessible(address, size);
		}

		sint32 CEXQuery(uint32 query, void* output, uint32 outputSize)
		{
			using namespace cemuextend::wire;
			if (query != static_cast<uint32>(Query::BridgeInfo) || outputSize < sizeof(BridgeInfo) ||
				!ValidateGuestRange(output, sizeof(BridgeInfo)))
				return static_cast<sint32>(Error::InvalidArgument);
			auto& info = *static_cast<BridgeInfo*>(output);
			std::memset(&info, 0, sizeof(info));
			info.available = 1;
			info.minimumAbiMajor = kAbiMajor;
			info.minimumAbiMinor = 0;
			info.maximumAbiMajor = kAbiMajor;
			info.maximumAbiMinor = kAbiMinor;
			info.maximumRegionSize = kMaximumRegionSize;
			info.hostBuildId = kBuildId;
			info.features = static_cast<uint64>(Feature::SharedMemory) |
				static_cast<uint64>(Feature::ServiceDirectory) |
				static_cast<uint64>(Feature::StatePages) |
				static_cast<uint64>(Feature::Bulk) |
				static_cast<uint64>(Feature::Permissions) |
				static_cast<uint64>(Feature::RawInput) |
				static_cast<uint64>(Feature::ObservedVpad);
			info.maximumServices = (kServiceDirectorySize - sizeof(ServiceDirectoryHeader)) /
				sizeof(ServiceDescriptor);
			return 0;
		}

		sint32 CEXRegister(uint32 abiVersion, void* region, uint32 regionSize, uint32be* outputSessionId)
		{
			using namespace cemuextend::wire;
			if (!ValidateGuestRange(outputSessionId, sizeof(*outputSessionId)) ||
				!ValidateGuestRange(region, regionSize))
				return static_cast<sint32>(Error::InvalidArgument);
			uint32 sessionId{};
			const auto result = BridgeHost::Instance().Register(abiVersion, region, regionSize, sessionId);
			if (result == 0)
				*outputSessionId = sessionId;
			return result;
		}

		sint32 CEXNotify(uint32 sessionId, uint32 flags)
		{
			return BridgeHost::Instance().Notify(sessionId, flags);
		}

		sint32 CEXUnregister(uint32 sessionId)
		{
			return BridgeHost::Instance().Unregister(sessionId);
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
		}

		void RPLUnmapped() override { BridgeHost::Instance().Stop(); }

		void rpl_entry(uint32, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Unloaded)
				BridgeHost::Instance().Stop();
		}
	};

	COSModule* GetModule()
	{
		static CemuExtendModule module;
		return &module;
	}
}
