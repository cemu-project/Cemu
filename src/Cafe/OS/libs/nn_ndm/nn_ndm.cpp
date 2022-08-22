#include "nn_ndm.h"
#include "Cafe/OS/common/OSCommon.h"

namespace nn
{
	namespace ndm
	{
		void nnNdmExport_GetDaemonStatus(PPCInterpreter_t* hCPU)
		{
			// parameters:
			// r3	pointer to status integer (out)
			// r4	daemon name (integer)
			forceLogDebug_printf("nn_ndm.GetDaemonStatus(...) - hack\n");
			// status codes:
			// 1 - running? Download Manager (scope.rpx) expects this to return 1 (or zero). Otherwise it will display downloads as disabled
			memory_writeU32(hCPU->gpr[3], 1);
			// 2 - running?
			// 3 - suspended?
			osLib_returnFromFunction(hCPU, 0);
		}

		void load()
		{
			osLib_addFunction("nn_ndm", "GetDaemonStatus__Q2_2nn3ndmFPQ4_2nn3ndm7IDaemon6StatusQ4_2nn3ndm4Cafe10DaemonName", nnNdmExport_GetDaemonStatus);
		}
	}
}