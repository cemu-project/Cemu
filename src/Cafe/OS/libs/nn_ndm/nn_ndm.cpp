#include "nn_ndm.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"

namespace nn
{
	namespace ndm
	{

		enum class DAEMON_NAME : uint32
		{
			UKN_0, // Boss related?
			UKN_1, // Download Manager? scope.rpx (Download Manager app) expects this to have status 0 or 1. Otherwise it will display downloads as disabled
			UKN_2,
		};

		enum class DAEMON_STATUS : uint32
		{
			STATUS_UKN_0 = 0, // probably: Ready or initializing?
			RUNNING = 1, // most likely running, but not 100% sure
			STATUS_UKN_2 = 2, // probably: ready, starting or something like that?
			SUSPENDED = 3,
		};

		constexpr size_t NUM_DAEMONS = 3;
		DAEMON_STATUS s_daemonStatus[NUM_DAEMONS];
		uint32 s_initializeRefCount;

		uint32 Initialize()
		{
			s_initializeRefCount++;
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NDM, 0);
		}

		uint32 IsInitialized()
		{
			return s_initializeRefCount != 0 ? 1 : 0;
		}

		uint32 Finalize()
		{
			if(s_initializeRefCount == 0)
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NDM, 0);
			s_initializeRefCount++;
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NDM, 0);
		}

		uint32 GetDaemonStatus(betype<DAEMON_STATUS>* statusOut, DAEMON_NAME daemonName)
		{
			size_t daemonIndex = (size_t)daemonName;
			if(daemonIndex >= NUM_DAEMONS)
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_NDM, 0);
			*statusOut = s_daemonStatus[daemonIndex];
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NDM, 0);
		}

		uint32 SuspendDaemons(uint32 daemonNameBitmask)
		{
			for(size_t i=0; i<NUM_DAEMONS; i++)
			{
				if(daemonNameBitmask & (1 << i))
					s_daemonStatus[i] = DAEMON_STATUS::SUSPENDED;
			}
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NDM, 0);
		}

		uint32 ResumeDaemons(uint32 daemonNameBitmask)
		{
			for(size_t i=0; i<NUM_DAEMONS; i++)
			{
				if(daemonNameBitmask & (1 << i))
					s_daemonStatus[i] = DAEMON_STATUS::RUNNING;
			}
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_NDM, 0);
		}

		void load()
		{
			for(size_t i=0; i<NUM_DAEMONS; i++)
				s_daemonStatus[i] = DAEMON_STATUS::RUNNING;
			s_initializeRefCount = 0;

			cafeExportRegisterFunc(Initialize, "nn_ndm", "Initialize__Q2_2nn3ndmFv", LogType::Placeholder);
			cafeExportRegisterFunc(Finalize, "nn_ndm", "Finalize__Q2_2nn3ndmFv", LogType::Placeholder);
			cafeExportRegisterFunc(IsInitialized, "nn_ndm", "IsInitialized__Q2_2nn3ndmFv", LogType::Placeholder);
			cafeExportRegisterFunc(GetDaemonStatus, "nn_ndm", "GetDaemonStatus__Q2_2nn3ndmFPQ4_2nn3ndm7IDaemon6StatusQ4_2nn3ndm4Cafe10DaemonName", LogType::Placeholder);
			cafeExportRegisterFunc(SuspendDaemons, "nn_ndm", "SuspendDaemons__Q2_2nn3ndmFUi", LogType::Placeholder);
			cafeExportRegisterFunc(ResumeDaemons, "nn_ndm", "ResumeDaemons__Q2_2nn3ndmFUi", LogType::Placeholder);
		}
	}
}
