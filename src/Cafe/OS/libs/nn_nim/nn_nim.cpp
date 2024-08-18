#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"
#include "Cafe/IOSU/legacy/iosu_nim.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/nn_common.h"

#define nimPrepareRequest() \
StackAllocator<iosu::nim::iosuNimCemuRequest_t> _buf_nimRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosu::nim::iosuNimCemuRequest_t* nimRequest = _buf_nimRequest.GetPointer(); \
ioBufferVector_t* nimBufferVector = _buf_bufferVector.GetPointer(); \
memset(nimRequest, 0, sizeof(iosu::nim::iosuNimCemuRequest_t)); \
memset(nimBufferVector, 0, sizeof(ioBufferVector_t)); \
nimBufferVector->buffer = (uint8*)nimRequest;

namespace nn
{
	namespace nim
	{

		void export_NeedsNetworkUpdate(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "NeedsNetworkUpdate() - placeholder");
			ppcDefineParamTypePtr(needsUpdate, uint8, 0);

			*needsUpdate = 0;

			osLib_returnFromFunction(hCPU, 0);
		}

		typedef struct  
		{
			uint32be ukn00;
			uint32be ukn04;
			uint32be ukn08;
			uint32be ukn0C;
			uint32be ukn10;
			uint32be ukn14;
			uint32be ukn18;
		}updatePackageProgress_t;

		void export_GetUpdatePackageProgress(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "GetUpdatePackageProgress() - placeholder");
			ppcDefineParamTypePtr(updatePackageProgress, updatePackageProgress_t, 0);
			// status of system update download
			// values are unknown
			updatePackageProgress->ukn00 = 0;
			updatePackageProgress->ukn04 = 0;
			updatePackageProgress->ukn18 = 0;

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_NeedsNotifyToUsers(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "NeedsNotifyToUsers() - placeholder");
			ppcDefineParamTypePtr(updatePackageProgress, updatePackageProgress_t, 0);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_GetNumTitlePackages(PPCInterpreter_t* hCPU)
		{
			nimPrepareRequest();

			nimRequest->requestCode = IOSU_NIM_GET_PACKAGE_COUNT;

			__depr__IOS_Ioctlv(IOS_DEVICE_NIM, IOSU_NIM_REQUEST_CEMU, 1, 1, nimBufferVector);

			uint32 numTitlePackages = nimRequest->resultU32.u32;

			osLib_returnFromFunction(hCPU, numTitlePackages);
		}

		static_assert(sizeof(iosu::nim::titlePackageInfo_t) == 0x50, "titlePackageInfo_t has invalid size");
		static_assert(offsetof(iosu::nim::titlePackageInfo_t, ukn28DLProgressRelatedMax_u64be) == 0x28, "ukn28_u64be has invalid offset");

		void export_ListTitlePackagesStatically(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "ListTitlePackagesStatically() - placeholder");
			ppcDefineParamTypePtr(titleIdList, uint64, 0);
			ppcDefineParamS32(maxCount, 1);

			nimPrepareRequest();
			
			nimRequest->requestCode = IOSU_NIM_GET_PACKAGES_TITLEID;
			nimRequest->maxCount = maxCount;
			nimRequest->ptr = (uint8*)(titleIdList);

			__depr__IOS_Ioctlv(IOS_DEVICE_NIM, IOSU_NIM_REQUEST_CEMU, 1, 1, nimBufferVector);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_GetTitlePackageInfos(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "GetTitlePackageInfos() - placeholder");
			ppcDefineParamTypePtr(titlePackageInfo, iosu::nim::titlePackageInfo_t, 0);
			ppcDefineParamTypePtr(titleIdList, uint64, 1);
			ppcDefineParamU32(count, 2);

			nimPrepareRequest();

			nimRequest->requestCode = IOSU_NIM_GET_PACKAGES_INFO;
			nimRequest->maxCount = count;
			nimRequest->ptr = (uint8*)(titleIdList);
			nimRequest->ptr2 = (uint8*)(titlePackageInfo);

			__depr__IOS_Ioctlv(IOS_DEVICE_NIM, IOSU_NIM_REQUEST_CEMU, 1, 1, nimBufferVector);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_NeedsNotifyToUsersTitlePackage(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "NeedsNotifyToUsers() - placeholder");
			ppcDefineParamTypePtr(titlePackageInfo, iosu::nim::titlePackageInfo_t, 0);

			osLib_returnFromFunction(hCPU, 0);
		}

		using IDBE_DATA = uint8[0x12060];

		void export_GetIconDatabaseEntries(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "GetIconDatabaseEntries() - placeholder");
			ppcDefineParamTypePtr(iconDatabaseEntries, IDBE_DATA, 0);
			ppcDefineParamTypePtr(titleIdList, uint64, 1);
			ppcDefineParamS32(count, 2);

			cemu_assert_debug(count == 1); // other count values are untested

			for (sint32 i = 0; i < count; i++)
			{
				nimPrepareRequest();

				nimRequest->requestCode = IOSU_NIM_GET_ICON_DATABASE_ENTRY;
				nimRequest->titleId = _swapEndianU64(titleIdList[i]);
				nimRequest->ptr = (uint8*)(iconDatabaseEntries + i);

				__depr__IOS_Ioctlv(IOS_DEVICE_NIM, IOSU_NIM_REQUEST_CEMU, 1, 1, nimBufferVector);

			}


			osLib_returnFromFunction(hCPU, 0);
		}

		void export_QuerySchedulerStatus(PPCInterpreter_t* hCPU)
		{
			cemuLog_logDebug(LogType::Force, "QuerySchedulerStatus() - placeholder");

			// scheduler status seems to be either a 4 byte array or 8 byte array (or structs)?
			// scope.rpx only checks the second byte and if it matches 0x01 then the scheduler is considered paused/stopped (displays that downloads are inactive)
			// men.rpx checks the first byte for == 1 and if true, it will show the download manager icon as downloading

			// downloads disabled:
			//memory_writeU32(hCPU->gpr[3], (0x00010000));
			// downloads enabled:
			memory_writeU32(hCPU->gpr[3], (0x00000000));

			osLib_returnFromFunction(hCPU, 0);
		}

		struct nimResultError
		{
			uint32be iosError;
			uint32be ukn04;
		};

		void ConstructResultError(nimResultError* resultError, uint32be* nimErrorCodePtr, uint32 uknParam)
		{
			uint32 nnResultCode = *nimErrorCodePtr;
			resultError->iosError = nnResultCode;
			resultError->ukn04 = uknParam;

			if (nnResultCode == 0xFFFFFFFF)
			{
				// not a valid code, used by a Wii U menu
				return;
			}

			// IOS errors need to be translated
			if ( (nnResultCode&0x18000000) == 0x18000000)
			{
				// alternative error format
				cemu_assert_unimplemented();
			}
			else
			{
				auto moduleId = nn::nnResult_GetModule(nnResultCode);
				if (moduleId == NN_RESULT_MODULE_NN_IOS)
				{
					// ios error
					cemu_assert_unimplemented();
				}
				else
				{
					// other error
					resultError->iosError = 0;
				}
			}
		}

		void export_GetECommerceInfrastructureCountry(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamU32BEPtr(country, 0);

			cemuLog_logDebug(LogType::Force, "GetECommerceInfrastructureCountry - todo");

			*country = 0;

			osLib_returnFromFunction(hCPU, 0);
		}

		typedef struct
		{
			betype<uint32> titleIdHigh;
			betype<uint32> titleIdLow;
			uint32 regionOrLanguageRelated;
			uint8 uknByte0C;
			uint8 applicationBoxDevice1; // this is what E-Shop app reads to determine device (0 -> mlc, 1 -> extern storage?)
			uint8 uknByte0E;
			uint8 applicationBoxDevice2;
			uint32 ukn10;
			uint8 uknByte14; // set to 0
			uint8 uknByte15; // set to 1
			uint8 postDownloadAction; // 0 -> ?, 1 -> ?, 2 -> Use bg install policy
			uint8 uknBytes17;
		}TitlePackageTaskConfig_t;

		static_assert(sizeof(TitlePackageTaskConfig_t) == 0x18, "");

		void export_MakeTitlePackageTaskConfigAutoUsingBgInstallPolicy(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamPtr(titlePackageTastConfig, TitlePackageTaskConfig_t, 0);
			ppcDefineParamU64(titleId, 2);
			ppcDefineParamU32(regionOrLanguage, 4);
			ppcDefineParamU32(uknR8, 5); // title type?

			cemuLog_logDebug(LogType::Force, "MakeTitlePackageTaskConfigAutoUsingBgInstallPolicy - placeholder");

			titlePackageTastConfig->titleIdHigh = (uint32)(titleId >> 32);
			titlePackageTastConfig->titleIdLow = (uint32)(titleId & 0xFFFFFFFF);
			titlePackageTastConfig->regionOrLanguageRelated = 0; // ?
			titlePackageTastConfig->uknByte0C = uknR8;
			titlePackageTastConfig->applicationBoxDevice1 = 1; // 1 -> mlc
			titlePackageTastConfig->applicationBoxDevice2 = 1; // 1 -> mlc

			titlePackageTastConfig->uknByte0E = 0; // ?
			titlePackageTastConfig->ukn10 = 0; // ?
			titlePackageTastConfig->uknByte14 = 0; // ?
			titlePackageTastConfig->uknByte15 = 1; // ?

			titlePackageTastConfig->postDownloadAction = 0; // ?

			titlePackageTastConfig->uknBytes17 = 0; // ?

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_CalculateTitleInstallSize(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(installSize, betype<uint64>, 0);
			ppcDefineParamPtr(titlePackageTastConfig, TitlePackageTaskConfig_t, 1);

			// get install size of currently installed title, otherwise return -1 as size
			cemuLog_logDebug(LogType::Force, "CalculateTitleInstallSize - todo");

			*installSize = 0xFFFFFFFFFFFFFFFF;

			osLib_returnFromFunction(hCPU, 0);
		}

		void load()
		{
			osLib_addFunction("nn_nim", "NeedsNetworkUpdate__Q2_2nn3nimFPb", export_NeedsNetworkUpdate);
			osLib_addFunction("nn_nim", "GetUpdatePackageProgress__Q2_2nn3nimFPQ3_2nn3nim21UpdatePackageProgress", export_GetUpdatePackageProgress);
			osLib_addFunction("nn_nim", "NeedsNotifyToUsers__Q3_2nn3nim4utilFPCQ3_2nn3nim21UpdatePackageProgress", export_NeedsNotifyToUsers);
			osLib_addFunction("nn_nim", "GetNumTitlePackages__Q2_2nn3nimFv", export_GetNumTitlePackages);

			osLib_addFunction("nn_nim", "GetTitlePackageInfos__Q2_2nn3nimFPQ3_2nn3nim16TitlePackageInfoPCULUi", export_GetTitlePackageInfos);
			osLib_addFunction("nn_nim", "NeedsNotifyToUsers__Q3_2nn3nim4utilFPCQ3_2nn3nim16TitlePackageInfoPCQ3_2nn3nim11ResultError", export_NeedsNotifyToUsersTitlePackage);

			osLib_addFunction("nn_nim", "ListTitlePackagesStatically__Q2_2nn3nimFPULUi", export_ListTitlePackagesStatically);

			osLib_addFunction("nn_nim", "GetECommerceInfrastructureCountry__Q2_2nn3nimFPQ3_2nn3nim7Country", export_GetECommerceInfrastructureCountry);


			osLib_addFunction("nn_nim", "QuerySchedulerStatus__Q2_2nn3nimFPQ3_2nn3nim15SchedulerStatus", export_QuerySchedulerStatus);

			osLib_addFunction("nn_nim", "GetIconDatabaseEntries__Q2_2nn3nimFPQ3_2nn3nim17IconDatabaseEntryPCULUi", export_GetIconDatabaseEntries);

			cafeExportRegisterFunc(ConstructResultError, "nn_nim", "Construct__Q3_2nn3nim11ResultErrorFQ2_2nn6Resulti", LogType::Placeholder);

			osLib_addFunction("nn_nim", "MakeTitlePackageTaskConfigAutoUsingBgInstallPolicy__Q3_2nn3nim4utilFULiQ3_2nn4Cafe9TitleType", export_MakeTitlePackageTaskConfigAutoUsingBgInstallPolicy);
			osLib_addFunction("nn_nim", "CalculateTitleInstallSize__Q2_2nn3nimFPLRCQ3_2nn3nim22TitlePackageTaskConfigPCUsUi", export_CalculateTitleInstallSize);

		}
	}
}
