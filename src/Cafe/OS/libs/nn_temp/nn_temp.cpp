#include "Cafe/OS/common/OSCommon.h"

namespace nn::temp
{
	uint64 tempIdGenerator = 0xdc1b04bd961f2c04ULL;

	void nnTempExport_TEMPCreateAndInitTempDir(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "TEMPCreateAndInitTempDir(...) - placeholder");

		// create random temp id
		memory_writeU64(hCPU->gpr[5], tempIdGenerator);
		tempIdGenerator = (tempIdGenerator << 3) | (tempIdGenerator >> 61);
		tempIdGenerator += 0x56e28bd5f4ULL;

		osLib_returnFromFunction(hCPU, 0);
	}

	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "nn_temp";
		}

		void RPLMapped() override
		{
			osLib_addFunction("nn_temp", "TEMPCreateAndInitTempDir", nnTempExport_TEMPCreateAndInitTempDir);
		};

	}s_COSnnTempModule;

	COSModule* GetModule()
	{
		return &s_COSnnTempModule;
	}
};

