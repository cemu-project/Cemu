#include "../PPCState.h"
#include "PPCInterpreterInternal.h"
#include "PPCInterpreterHelper.h"

std::unordered_set<std::string> sUnsupportedHLECalls;

void PPCInterpreter_handleUnsupportedHLECall(PPCInterpreter_t* hCPU)
{
	const char* libFuncName = (char*)memory_getPointerFromVirtualOffset(hCPU->instructionPointer + 8);
	std::string tempString = fmt::format("Unsupported lib call: {}", libFuncName);
	if (sUnsupportedHLECalls.find(tempString) == sUnsupportedHLECalls.end())
	{
		cemuLog_log(LogType::UnsupportedAPI, "{}", tempString);
		sUnsupportedHLECalls.emplace(tempString);
	}
	hCPU->gpr[3] = 0;
	PPCInterpreter_nextInstruction(hCPU);
}

std::vector<void(*)(PPCInterpreter_t* hCPU)>* sPPCHLETable{};

HLEIDX PPCInterpreter_registerHLECall(HLECALL hleCall, std::string hleName)
{
	if (!sPPCHLETable)
		sPPCHLETable = new std::vector<void(*)(PPCInterpreter_t* hCPU)>();
	for (sint32 i = 0; i < sPPCHLETable->size(); i++)
	{
		if ((*sPPCHLETable)[i] == hleCall)
			return i;
	}
	HLEIDX newFuncIndex = (sint32)sPPCHLETable->size();
	sPPCHLETable->resize(sPPCHLETable->size() + 1);
	(*sPPCHLETable)[newFuncIndex] = hleCall;
	return newFuncIndex;
}

HLECALL PPCInterpreter_getHLECall(HLEIDX funcIndex)
{
	if (funcIndex < 0 || funcIndex >= sPPCHLETable->size())
		return nullptr;
	return sPPCHLETable->data()[funcIndex];
}

std::mutex g_hleLogMutex;

void PPCInterpreter_virtualHLE(PPCInterpreter_t* hCPU, unsigned int opcode)
{
	uint32 hleFuncId = opcode & 0xFFFF;
	if (hleFuncId == 0xFFD0)
	{
		g_hleLogMutex.lock();
		PPCInterpreter_handleUnsupportedHLECall(hCPU);
		g_hleLogMutex.unlock();
		return;
	}
	else
	{
		// os lib function
		cemu_assert(hleFuncId < sPPCHLETable->size());
		auto hleCall = (*sPPCHLETable)[hleFuncId];
		cemu_assert(hleCall);
		hleCall(hCPU);
	}
}