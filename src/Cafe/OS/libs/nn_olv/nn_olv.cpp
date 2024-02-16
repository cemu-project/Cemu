#include "nn_olv.h"

#include "nn_olv_InitializeTypes.h"
#include "nn_olv_UploadCommunityTypes.h"
#include "nn_olv_DownloadCommunityTypes.h"
#include "nn_olv_UploadFavoriteTypes.h"
#include "nn_olv_PostTypes.h"
#include "nn_olv_OfflineDB.h"

#include "Cafe/OS/libs/proc_ui/proc_ui.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

namespace nn
{
	namespace olv
	{
		struct PortalAppParam_t
		{
			/* +0x1A663B */ char serviceToken[32]; // size is unknown
		};

		void exportPortalAppParam_GetServiceToken(PPCInterpreter_t* hCPU)
		{
			// r3 = PortalAppParam
			ppcDefineParamTypePtr(portalAppParam, PortalAppParam_t, 0);

			strcpy(portalAppParam->serviceToken, "servicetoken");
			// this token is probably just the act IndependentServiceToken for the Miiverse title?

			osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(&portalAppParam->serviceToken));
		}

		static SysAllocator<OSThread_t> s_OlvReleaseBgThread;
		SysAllocator<uint8, 1024> s_OlvReleaseBgThreadStack;
		SysAllocator<char, 32> s_OlvReleaseBgThreadName;

		void StubPostAppReleaseBackground(PPCInterpreter_t* hCPU)
		{
			coreinit::OSSleepTicks(ESPRESSO_TIMER_CLOCK * 2); // Sleep 2s
			ProcUI_SendForegroundMessage();
		}

		sint32 StubPostApp(void* pAnyPostParam)
		{
			coreinit::OSCreateThreadType(s_OlvReleaseBgThread.GetPtr(), RPLLoader_MakePPCCallable(StubPostAppReleaseBackground), 0, nullptr, s_OlvReleaseBgThreadStack.GetPtr() + s_OlvReleaseBgThreadStack.GetByteSize(), (sint32)s_OlvReleaseBgThreadStack.GetByteSize(), 0, (1 << 1) | (1 << 3), OSThread_t::THREAD_TYPE::TYPE_APP);
			coreinit::OSResumeThread(s_OlvReleaseBgThread.GetPtr());
			strcpy(s_OlvReleaseBgThreadName.GetPtr(), "StubPostApp!");
			coreinit::OSSetThreadName(s_OlvReleaseBgThread.GetPtr(),s_OlvReleaseBgThreadName.GetPtr());
			return OLV_RESULT_SUCCESS;
		}

		sint32 StubPostAppResult()
		{
			return OLV_RESULT_STATUS(301); // Cancelled post app
		}

		// Somehow required, MK8 doesn't even seem to care about the error codes lol
		char* UploadedPostData_GetPostId(char* pPostData)
		{
			pPostData[4] = '\0';
			return &pPostData[4];
		}

		// https://github.com/kinnay/NintendoClients/wiki/Wii-U-Error-Codes#act-error-codes
		constexpr uint32 GetErrorCodeImpl(uint32 in)
		{
			uint32_t errorCode = in;
			uint32_t errorVersion = (errorCode >> 27) & 3;
			uint32_t errorModuleMask = (errorVersion != 3) ? 0x1FF00000 : 0x7F00000;
			bool isCodeFailure = errorCode & 0x80000000;

			if (((errorCode & errorModuleMask) >> 20) == NN_RESULT_MODULE_NN_ACT)
			{
				// BANNED_ACCOUNT_IN_INDEPENDENT_SERVICE or BANNED_ACCOUNT_IN_INDEPENDENT_SERVICE_TEMPORARILY
				if (errorCode == OLV_ACT_RESULT_STATUS(2805) || errorCode == OLV_ACT_RESULT_STATUS(2825))
				{
					uint32 tmpCode = OLV_RESULT_STATUS(1008);
					return GetErrorCodeImpl(tmpCode);
				}
				// BANNED_DEVICE_IN_INDEPENDENT_SERVICE or  BANNED_DEVICE_IN_INDEPENDENT_SERVICE_TEMPORARILY
				else if (errorCode == OLV_ACT_RESULT_STATUS(2815) || errorCode == OLV_ACT_RESULT_STATUS(2835))
				{
					uint32 tmpCode = OLV_RESULT_STATUS(1009);
					return GetErrorCodeImpl(tmpCode);
				}
				else
				{
					// Check ACT error code
					return 1159999;
				}
			}
			else
			{
				if (((errorCode & errorModuleMask) >> 20) == NN_RESULT_MODULE_NN_OLV && isCodeFailure)
				{
					uint32_t errorValueMask = (errorVersion != 3) ? 0xFFFFF : 0x3FF;
					return ((errorCode & errorValueMask) >> 7) + 1150000;
				}
				else
				{
					return 1159999;
				}
			}
		}

		uint32 GetErrorCode(uint32be* pResult)
		{
			return GetErrorCodeImpl(pResult->value());
		}

		static_assert(GetErrorCodeImpl(0xa119c600) == 1155004);

		void load()
		{
			g_ReportTypes = 0;
			g_IsOnlineMode = false;
			g_IsInitialized = false;
			g_IsOfflineDBMode = false;

			loadOliveInitializeTypes();
			loadOliveUploadCommunityTypes();
			loadOliveDownloadCommunityTypes();
			loadOliveUploadFavoriteTypes();
			loadOlivePostAndTopicTypes();

			cafeExportRegisterFunc(GetErrorCode, "nn_olv", "GetErrorCode__Q2_2nn3olvFRCQ2_2nn6Result", LogType::NN_OLV);

			osLib_addFunction("nn_olv", "GetServiceToken__Q4_2nn3olv6hidden14PortalAppParamCFv", exportPortalAppParam_GetServiceToken);

			cafeExportRegisterFunc(StubPostApp, "nn_olv", "UploadPostDataByPostApp__Q2_2nn3olvFPCQ3_2nn3olv28UploadPostDataByPostAppParam", LogType::NN_OLV);
			cafeExportRegisterFunc(StubPostApp, "nn_olv", "UploadCommentDataByPostApp__Q2_2nn3olvFPCQ3_2nn3olv31UploadCommentDataByPostAppParam", LogType::NN_OLV);
			cafeExportRegisterFunc(StubPostApp, "nn_olv", "UploadDirectMessageDataByPostApp__Q2_2nn3olvFPCQ3_2nn3olv37UploadDirectMessageDataByPostAppParam", LogType::NN_OLV);

			cafeExportRegisterFunc(StubPostAppResult, "nn_olv", "GetResultByPostApp__Q2_2nn3olvFv", LogType::NN_OLV);
			cafeExportRegisterFunc(StubPostAppResult, "nn_olv", "GetResultWithUploadedPostDataByPostApp__Q2_2nn3olvFPQ3_2nn3olv16UploadedPostData", LogType::NN_OLV);
			cafeExportRegisterFunc(StubPostAppResult, "nn_olv", "GetResultWithUploadedDirectMessageDataByPostApp__Q2_2nn3olvFPQ3_2nn3olv25UploadedDirectMessageData", LogType::NN_OLV);
			cafeExportRegisterFunc(StubPostAppResult, "nn_olv", "GetResultWithUploadedCommentDataByPostApp__Q2_2nn3olvFPQ3_2nn3olv19UploadedCommentData", LogType::NN_OLV);

			cafeExportRegisterFunc(UploadedPostData_GetPostId, "nn_olv", "GetPostId__Q3_2nn3olv16UploadedPostDataCFv", LogType::NN_OLV);
		}

		void unload() // not called yet
		{
			OfflineDB_Shutdown();
		}

	}
}