#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/cemuextend.h"
#include "Cafe/OS/libs/cemuextend/BridgeHost.h"
#include "Cafe/OS/libs/cemuextend/BuildId.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"

#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/HW/Espresso/CemodRuntime.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "cemuextend/transport.hpp"

namespace cemuextend_hle
{
	namespace
	{
		constexpr const char* kLibraryName = "cemuextend";

		template<ModMemoryPermission Permission>
		std::byte* ResolveSandbox(PPCInterpreter_t* hCPU, uint32 address, uint32 size)
		{
			if (!hCPU->modExecutionContext || !address || !size)
				return nullptr;
			return hCPU->modExecutionContext->Resolve(address, size, Permission);
		}

		void Return(PPCInterpreter_t* hCPU, cemuextend::wire::Error result)
		{
			osLib_returnFromFunction(hCPU, static_cast<uint32>(static_cast<sint32>(result)));
		}

		void CEX2Query(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext || hCPU->gpr[5] < sizeof(transport::Info))
				return Return(hCPU, wire::Error::PermissionDenied);
			auto* output = ResolveSandbox<ModMemoryPermission::Write>(hCPU, hCPU->gpr[4], sizeof(transport::Info));
			if (!output)
				return Return(hCPU, wire::Error::InvalidArgument);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Query(
				*hCPU->modExecutionContext, hCPU->gpr[3], {output, sizeof(transport::Info)})));
		}

		void CEX2Open(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext || hCPU->gpr[4] != sizeof(transport::OpenOptions))
				return Return(hCPU, wire::Error::PermissionDenied);
			auto* options = ResolveSandbox<ModMemoryPermission::Read>(hCPU, hCPU->gpr[3], hCPU->gpr[4]);
			auto* output = ResolveSandbox<ModMemoryPermission::Write>(hCPU, hCPU->gpr[5], sizeof(wire::Be32));
			if (!options || !output)
				return Return(hCPU, wire::Error::InvalidArgument);
			uint32 session{};
			const auto result = static_cast<wire::Error>(Cex2Host::Instance().Open(
				*hCPU->modExecutionContext, {options, hCPU->gpr[4]}, session));
			if (result == wire::Error::Ok)
				*reinterpret_cast<wire::Be32*>(output) = session;
			Return(hCPU, result);
		}

		void CEX2Submit(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext || hCPU->gpr[5] > transport::kMaximumMessageSize)
				return Return(hCPU, wire::Error::PermissionDenied);
			auto* request = ResolveSandbox<ModMemoryPermission::Read>(hCPU, hCPU->gpr[4], hCPU->gpr[5]);
			if (!request)
				return Return(hCPU, wire::Error::InvalidArgument);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Submit(
				*hCPU->modExecutionContext, hCPU->gpr[3], {request, hCPU->gpr[5]})));
		}

		void CEX2Poll(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext || hCPU->gpr[5] > transport::kMaximumMessageSize)
				return Return(hCPU, wire::Error::PermissionDenied);
			auto* output = ResolveSandbox<ModMemoryPermission::Write>(hCPU, hCPU->gpr[4], hCPU->gpr[5]);
			auto* sizeOutput = ResolveSandbox<ModMemoryPermission::Write>(hCPU, hCPU->gpr[6], sizeof(wire::Be32));
			if (!output || !sizeOutput)
				return Return(hCPU, wire::Error::InvalidArgument);
			uint32 outputSize{};
			const auto result = static_cast<wire::Error>(Cex2Host::Instance().Poll(
				*hCPU->modExecutionContext, hCPU->gpr[3], {output, hCPU->gpr[5]}, outputSize));
			*reinterpret_cast<wire::Be32*>(sizeOutput) = outputSize;
			Return(hCPU, result);
		}

		void CEX2Cancel(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext)
				return Return(hCPU, wire::Error::PermissionDenied);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Cancel(
				*hCPU->modExecutionContext, hCPU->gpr[3], hCPU->gpr[4])));
		}

		void CEX2Close(PPCInterpreter_t* hCPU)
		{
			using namespace cemuextend;
			if (!hCPU->modExecutionContext)
				return Return(hCPU, wire::Error::PermissionDenied);
			Return(hCPU, static_cast<wire::Error>(Cex2Host::Instance().Close(
				*hCPU->modExecutionContext, hCPU->gpr[3])));
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
		}

		void RPLUnmapped() override
		{
			GetCemodRuntime().UnloadAll();
			Cex2Host::Instance().CloseAll();
			BridgeHost::Instance().Stop();
		}

		void rpl_entry(uint32, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Unloaded)
			{
				GetCemodRuntime().UnloadAll();
				Cex2Host::Instance().CloseAll();
				BridgeHost::Instance().Stop();
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
}
